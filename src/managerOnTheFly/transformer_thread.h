#ifndef TRANSFORMER_THREAD_H
#define TRANSFORMER_THREAD_H

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

#include <iostream>
#include <yarp/os/Bottle.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Port.h>
#include <yarp/os/RateThread.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/Semaphore.h>
#include <yarp/sig/Image.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;

class TransformerThread: public RateThread
{
public:
    TransformerThread(ResourceFinder &_rf);

    bool threadInit();
    void run();
    bool set_current_class(string _current_class);
    bool set_true_class(string _true_class);
    bool set_mode(int _mode);
    bool set_state(int _state);
    bool get_current_class(string &_current_class);
    bool get_true_class(string &_true_class);
    bool execReq(const Bottle &command, Bottle &reply);
    void interrupt();
    void threadRelease();
    bool interruptCoding();
    bool resumeCoding();
    
private:
    ResourceFinder                      &rf;
    Semaphore                           mutex;
    bool                                verbose;

    //input
    BufferedPort<Image>                 port_in_img;
    BufferedPort<Bottle>                port_in_blobs;

    //output
    Port                                port_out_show;
    Port                                port_out_crop;
    Port                                port_out_img;
    Port                                port_out_imginfo;
    
    //rpc
    RpcClient                           port_rpc_are_get_hand;

    int                                 radius_crop; 
    int                                 radius_crop_robot;
    int                                 radius_crop_human;
    ImageOf<PixelRgb>                   img_crop;
    string                              current_class;
    string                              true_class;
    bool                                coding_interrupted;
    int                                 mode;
    int                                 state;
    double                              blink_init_time;
    double                              blink_visible_time;
    double                              blink_invisible_time;
};

#endif
