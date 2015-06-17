/* 
 * Copyright (C) 2011 Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
 * Author: Carlo Ciliberto
 * email:  carlo.ciliberto@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#ifndef STORER_THREAD_H
#define STORER_THREAD_H

#include <list>
#include <string>

#include <yarp/os/Bottle.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Port.h>
#include <yarp/os/RateThread.h>
#include <yarp/os/Semaphore.h>

class StorerThread: public yarp::os::RateThread
{
public:
    StorerThread(yarp::os::ResourceFinder &_rf);
    bool threadInit();
    void run();
    bool set_current_class(std::string _current_class);
    bool get_current_class(std::string &_current_class);
    bool reset_scores();
    bool set_mode(int _mode);
    bool set_state(int _state);
    bool execReq(const yarp::os::Bottle &command, yarp::os::Bottle &reply);
    void interrupt();
    void threadRelease();
    
private:
    yarp::os::ResourceFinder                    &rf;
    yarp::os::Semaphore                         mutex;
    bool                                		verbose;

    //input
    yarp::os::BufferedPort<yarp::os::Bottle>	port_in_scores;

    //output
    yarp::os::Port                              port_out_confidence;

    int                                 		bufferSize;
    std::list<yarp::os::Bottle>                 scores_buffer;

    std::string                              	current_class;

    int                                 		mode;
    int                                 		state;

    int                                 		confidence_width;
    int                                 		confidence_height;
};

#endif
