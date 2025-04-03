/*
 * Copyright (c) 2023 Conner Vieira - V0LT.
 *
 * Phantom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License
 * version 3 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdio>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "tclap/CmdLine.h"
#include "support/filesystem.h"
#include "support/timing.h"
#include "support/platform.h"
#include "video/videobuffer.h"
#include "motiondetector.h"
#include "alpr.h"

// Required for random string generation (random_string):
#include <string>
#include <random>

// Required for deleting old files (remove_old_files):
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <cstring>

// This function generates a string of random characters of a given length:
std::string random_string(size_t length) {
    const std::string characters = "0123456789abcdefghijklmnopqrstuvwxyz"; // Define the list of characters to choose from.
    std::string random_string;
    
    // Create a random device and a random number generator:
    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 generator(rd()); // Seed the generator
    std::uniform_int_distribution<> distribution(0, characters.size() - 1); // Define the range

    // Generate random characters:
    for (size_t i = 0; i < length; ++i) {
        random_string += characters[distribution(generator)];
    }

    return random_string;
}

// This function deletes all files from a given directory older than a given age in seconds:
void remove_old_files(const std::string& directory, int age_seconds) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
        std::cerr << "{\"error\": \"Error opening directory: " << strerror(errno) << "\"}" << std::endl;
        return;
    }

    struct dirent* entry;
    time_t now = time(nullptr);
    time_t threshold = now - age_seconds; // Determine the threshold, under which files will be deleted.

    while ((entry = readdir(dir)) != nullptr) { // Iterate over each file in the directory.
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) { // Skip the "." and ".." entries.
            continue;
        }

        std::string filePath = directory + "/" + entry->d_name;
        struct stat fileStat;

        if (stat(filePath.c_str(), &fileStat) == 0) {
            if (fileStat.st_mtime < threshold) { // Check of the file is older than the threshold.
                if (!remove(filePath.c_str()) == 0) {
                    std::cerr << "{\"error\": \"Error deleting file: " << filePath << " - " << strerror(errno) << "\"}" << std::endl;
                }
            }
        } else {
            std::cerr << "{\"error\": \"Error getting file status: " << filePath << " - " << strerror(errno) << "\"}" << std::endl;
        }
    }

    closedir(dir);
}

using namespace alpr;

const std::string MAIN_WINDOW_NAME = "ALPR main window";

const bool SAVE_LAST_VIDEO_STILL = false;
const std::string LAST_VIDEO_STILL_LOCATION = "/tmp/laststill.jpg";
const std::string WEBCAM_PREFIX = "/dev/video";
MotionDetector motiondetector;
bool do_motiondetection = true;
bool save_each_frame = false;

/** Function Headers */
bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool save_each_frame);
bool is_supported_video(std::string file_name);
bool is_supported_image(std::string file_name);

std::string templatePattern;

// This boolean is set to false when the user hits terminates (e.g., CTRL+C )
// so we can end infinite loops for things like video processing.
bool program_active = true;

int main(int argc, const char** argv) {
    std::vector<std::string> filenames;
    std::string configFile = "";
    int seektoms = 0;
    std::string country;
    int topn;

    TCLAP::CmdLine cmd("Phantom ALPR", ' ', Alpr::getVersion());

    TCLAP::UnlabeledMultiArg<std::string>  fileArg("image_file", "Image containing license plates", true, "", "image_file_path");


    TCLAP::ValueArg<std::string> countryCodeArg("c","country","Country code to identify (Ex: 'us' for USA or 'eu' for Europe).  Default=us", false, "us", "country_code");
    TCLAP::ValueArg<int> topNArg("n","topn","Max number of possible plate numbers to return.  Default=10",false, 10 ,"topN");
    TCLAP::SwitchArg motiondetect("", "motion", "Use motion detection on video file or stream.", cmd, false);
    TCLAP::SwitchArg saveframes("s", "save_frames", "Save each frame to `/dev/shm/phantomalpr/` with a unique identifier.", cmd, false);

    try {
        cmd.add( topNArg );
        cmd.add( fileArg );
        cmd.add( countryCodeArg );

        if (cmd.parse( argc, argv ) == false) {
            // Error occurred while parsing. Exit now.
            return 1;
        }

        filenames = fileArg.getValue();

        country = countryCodeArg.getValue();
        topn = topNArg.getValue();
        do_motiondetection = motiondetect.getValue();
        save_each_frame = saveframes.getValue();
    } catch (TCLAP::ArgException &e) {
        std::cerr << "{\"error\": \"" << e.error() << " for arg " << e.argId() << "\"}" << std::endl;
        return 1;
    }


    cv::Mat frame;

    Alpr alpr(country, configFile);
    alpr.setTopN(topn);

    alpr.getConfig()->setDebug(false);

    alpr.setDetectRegion(true);

    if (alpr.isLoaded() == false) {
        std::cerr << "{\"error\": \"Error loading Phantom\"}" << std::endl;
        return 1;
    }

    if (save_each_frame) { // If individual frame-saving is enabled, then initialize the corresponding output directory.
        const char* frame_directory = "/dev/shm/phantomalpr"; // This is the directory where each individual still frame will be saved.
        mode_t permissions = 0777; // Set permissions to read, write, and execute for everyone.
        makePath(frame_directory, permissions); // Create the directory.
    }


    for (unsigned int i = 0; i < filenames.size(); i++) { // Iterate through all of the file names supplied.
        std::string filename = filenames[i];

        if (filename == "webcam" || startsWith(filename, WEBCAM_PREFIX)) { // Handle webcam video streams.
            int webcamnumber = 0;
      
            // Parse the webcam device number.
            if(startsWith(filename, WEBCAM_PREFIX) && filename.length() > WEBCAM_PREFIX.length()) { 
                webcamnumber = atoi(filename.substr(WEBCAM_PREFIX.length()).c_str());
            }
      
            int framenum = 0;
            cv::VideoCapture cap(webcamnumber);
            if (!cap.isOpened()) {
                std::cerr << "{\"error\": \"Error opening webcam\"}" << std::endl;
                return 1;
            }
      
            while (cap.read(frame)) {
                if (framenum == 0) {
                    motiondetector.ResetMotionDetection(&frame);
                }
                detectandshow(&alpr, frame, "", save_each_frame);
                sleep_ms(10);
                framenum++;
            }
        } else if (is_supported_video(filename)) { // Handle video files.
            if (fileExists(filename.c_str())) {
                int framenum = 0;

                cv::VideoCapture cap = cv::VideoCapture();
                cap.open(filename);
                cap.set(cv::CAP_PROP_POS_MSEC, seektoms);

                while (cap.read(frame)) {
                    if (SAVE_LAST_VIDEO_STILL) {
                        cv::imwrite(LAST_VIDEO_STILL_LOCATION, frame);
                    }
              
                    if (framenum == 0)
                        motiondetector.ResetMotionDetection(&frame);
                    detectandshow(&alpr, frame, "", save_each_frame);
                    sleep_ms(1); // Create a 1 millisecond delay.
                    framenum++;
                }
            } else {
                std::cerr << "{\"error\": \"Video file not found: " << filename << "\"}" << std::endl;
            }
        } else if (is_supported_image(filename)) { // Handle image files.
            if (fileExists(filename.c_str())) {
                frame = cv::imread(filename);

                bool plate_found = detectandshow(&alpr, frame, "", save_each_frame);

            } else {
                std::cout << "{\"error\": \"Image file not found: " << filename << "\"}" << std::endl;
            }

        } else if (DirectoryExists(filename.c_str())) { // Handle directories.
            std::vector<std::string> files = getFilesInDir(filename.c_str());

            std::sort(files.begin(), files.end(), stringCompare);

            for (int i = 0; i < files.size(); i++) {
                if (is_supported_image(files[i])) {
                    std::string fullpath = filename + "/" + files[i];
                    std::cout << fullpath << std::endl;
                    frame = cv::imread(fullpath.c_str());
                }
            }

        } else  {
            std::cerr << "{\"error\": \"Unknown file type\"}" << std::endl;
            return 1;
        }
    }

    return 0;
}





bool is_supported_video(std::string file_name) {
    return (hasEndingInsensitive(file_name, ".avi") || hasEndingInsensitive(file_name, ".mp4") || hasEndingInsensitive(file_name, ".webm") || hasEndingInsensitive(file_name, ".flv") || hasEndingInsensitive(file_name, ".mjpg") || hasEndingInsensitive(file_name, ".mjpeg") || hasEndingInsensitive(file_name, ".mkv") || hasEndingInsensitive(file_name, ".m4v") || hasEndingInsensitive(file_name, ".ts"));
}

bool is_supported_image(std::string file_name) {
    return (hasEndingInsensitive(file_name, ".png") || hasEndingInsensitive(file_name, ".jpg") || hasEndingInsensitive(file_name, ".tif") || hasEndingInsensitive(file_name, ".bmp") ||  hasEndingInsensitive(file_name, ".jpeg") || hasEndingInsensitive(file_name, ".gif"));
}

bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool save_each_frame) {
    // Get the time that the analysis started:
    timespec startTime;
    getTimeMonotonic(&startTime);


    std::vector<AlprRegionOfInterest> regionsOfInterest;
    if (do_motiondetection) {
        cv::Rect rectan = motiondetector.MotionDetect(&frame);
        if (rectan.width > 0) {
            regionsOfInterest.push_back(AlprRegionOfInterest(rectan.x, rectan.y, rectan.width, rectan.height));
        }
    } else {
        regionsOfInterest.push_back(AlprRegionOfInterest(0, 0, frame.cols, frame.rows));
    }

    AlprResults results;
    if (regionsOfInterest.size() > 0) {
        results = alpr->recognize(frame.data, frame.elemSize(), frame.cols, frame.rows, regionsOfInterest);
        remove_old_files("/dev/shm/phantomalpr/", 10); // Remove all frames older than 10 seconds.
        results.identifier = random_string(12); // Assign a random identifier to this set of results.
        imwrite("/dev/shm/phantom-webcam.jpg", frame); // Save the captured frame to memory so other program's can access it.
        if (save_each_frame) {
            imwrite("/dev/shm/phantomalpr/" + results.identifier + ".jpg", frame);
        }
    }


    // Get the time that the analysis finished:
    timespec endTime;
    getTimeMonotonic(&endTime);

    // Calculate the total processing time based on the start time and end time:
    double totalProcessingTime = diffclock(startTime, endTime);

    std::cout << alpr->toJson(results) << std::endl; // Print the analysis results in JSON format.

    return results.plates.size() > 0; // Return 'true' if plates were detected.
}

