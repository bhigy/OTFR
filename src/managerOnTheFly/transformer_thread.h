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

class TransformerThread: public yarp::os::RateThread
{
public:
    TransformerThread(yarp::os::ResourceFinder &_rf);

    bool threadInit();
    void run();
    bool set_current_class(std::string _current_class);
    bool set_true_class(std::string _true_class);
    bool set_mode(int _mode);
    bool set_state(int _state);
    bool get_current_class(std::string &_current_class);
    bool get_true_class(std::string &_true_class);
    bool execReq(const yarp::os::Bottle &command, yarp::os::Bottle &reply);
    void interrupt();
    void threadRelease();
    bool interruptCoding();
    bool resumeCoding();
    
private:
    yarp::os::ResourceFinder					&rf;
    yarp::os::Semaphore							mutex;
    bool										verbose;

    //input
    yarp::os::BufferedPort<yarp::sig::Image>	port_in_img;
    yarp::os::BufferedPort<yarp::os::Bottle>    port_in_blobs;

    //output
    yarp::os::Port								port_out_show;
    yarp::os::Port								port_out_crop;
    yarp::os::Port								port_out_img;
    yarp::os::Port								port_out_imginfo;
    
    //rpc
    yarp::os::RpcClient							port_rpc_are_get_hand;

    int											radius_crop; 
    int											radius_crop_robot;
    int											radius_crop_human;
    yarp::sig::ImageOf<yarp::sig::PixelRgb>		img_crop;
    std::string									current_class;
    std::string									true_class;
    bool										coding_interrupted;
    int											mode;
    int											state;
    double										blink_init_time;
    double										blink_visible_time;
    double										blink_invisible_time;
};

#endif
