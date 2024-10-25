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

using namespace alpr;

const std::string MAIN_WINDOW_NAME = "ALPR main window";

const bool SAVE_LAST_VIDEO_STILL = false;
const std::string LAST_VIDEO_STILL_LOCATION = "/tmp/laststill.jpg";
const std::string WEBCAM_PREFIX = "/dev/video";
MotionDetector motiondetector;
bool do_motiondetection = true;

/** Function Headers */
bool detectandshow(Alpr* alpr, cv::Mat frame, std::string region, bool writeJson);
bool is_supported_video(std::string file_name);
bool is_supported_image(std::string file_name);

std::string templatePattern;

// This boolean is set to false when the user hits terminates (e.g., CTRL+C )
// so we can end infinite loops for things like video processing.
bool program_active = true;

int main( int argc, const char** argv) {
    std::vector<std::string> filenames;
    std::string configFile = "";
    bool outputJson = true;
    int seektoms = 0;
    std::string country;
    int topn;

    TCLAP::CmdLine cmd("Phantom ALPR", ' ', Alpr::getVersion());

    TCLAP::UnlabeledMultiArg<std::string>  fileArg( "image_file", "Image containing license plates", true, "", "image_file_path"  );


    TCLAP::ValueArg<std::string> countryCodeArg("c","country","Country code to identify (either us for USA or eu for Europe).  Default=us",false, "us" ,"country_code");
    TCLAP::ValueArg<int> topNArg("n","topn","Max number of possible plate numbers to return.  Default=10",false, 10 ,"topN");
    TCLAP::SwitchArg motiondetect("", "motion", "Use motion detection on video file or stream.  Default=off", cmd, false);

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
                detectandshow(&alpr, frame, "", outputJson);
                imwrite("/dev/shm/phantom-webcam.jpg", frame); // Save the captured frame to memory so other program's can access it.
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
                    detectandshow(&alpr, frame, "", outputJson);
                    sleep_ms(1); // Create a 1 millisecond delay.
                    framenum++;
                }
            } else {
                std::cerr << "{\"error\": \"Video file not found: " << filename << "\"}" << std::endl;
            }
        } else if (is_supported_image(filename)) { // Handle image files.
            if (fileExists(filename.c_str())) {
                frame = cv::imread(filename);

                bool plate_found = detectandshow(&alpr, frame, "", outputJson);

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

bool detectandshow( Alpr* alpr, cv::Mat frame, std::string region, bool writeJson) {
    // Get the time that the analysis started.
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
    }

    // Get the time that the analysis finished.
    timespec endTime;
    getTimeMonotonic(&endTime);

    // Calculate the total processing time based on the start time and end time.
    double totalProcessingTime = diffclock(startTime, endTime);

    std::cout << alpr->toJson(results) << std::endl; // Print the analysis results in JSON format.

    return results.plates.size() > 0; // Return 'true' if plates were detected.
}

