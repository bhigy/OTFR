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

using namespace std;
using namespace yarp::os;

class StorerThread: public RateThread
{
public:
    StorerThread(ResourceFinder &_rf);
    bool threadInit();
    void run();
    bool set_current_class(string _current_class);
    bool get_current_class(string &_current_class);
    bool reset_scores();
    bool set_mode(int _mode);
    bool set_state(int _state);
    bool execReq(const Bottle &command, Bottle &reply);
    void interrupt();
    void threadRelease();
    
private:
    ResourceFinder                      &rf;
    Semaphore                           mutex;
    bool                                verbose;

    //input
    BufferedPort<Bottle>                port_in_scores;

    //output
    Port                                port_out_confidence;

    int                                 bufferSize;
    list<Bottle>                        scores_buffer;

    string                              current_class;

    int                                 mode;
    int                                 state;

    int                                 confidence_width;
    int                                 confidence_height;
};

#endif
