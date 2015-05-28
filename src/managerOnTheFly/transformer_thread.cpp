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

#include <algorithm>

#include <cv.h>
#include <yarp/os/Time.h>
#include <yarp/os/Stamp.h>
#include <yarp/os/ResourceFinder.h>

#include "constants.h"
#include "transformer_thread.h"

TransformerThread::TransformerThread(ResourceFinder &_rf) : RateThread(5),rf(_rf)
{
}

bool TransformerThread::threadInit()
{
    verbose=rf.check("verbose");

    string name=rf.find("name").asString().c_str();

    radius_crop_robot=rf.check("radius_crop_robot",Value(80)).asInt();
    radius_crop_human=rf.check("radius_crop_human",Value(40)).asInt();
    radius_crop=radius_crop_human;

    //Ports
    //-----------------------------------------------------------
    //input
    port_in_img.open(("/"+name+"/img:i").c_str());
    port_in_blobs.open(("/"+name+"/blobs:i").c_str());

    //output
    port_out_show.open(("/"+name+"/show:o").c_str());
    port_out_crop.open(("/"+name+"/crop:o").c_str());
//////////////////////////////////////////////////////////////////////////////////////////////////////
    port_out_img.open(("/"+name+"/img:o").c_str());
    port_out_imginfo.open(("/"+name+"/imginfo:o").c_str());
//////////////////////////////////////////////////////////////////////////////////////////////////////

    //rpc
    port_rpc_are_get_hand.open(("/"+name+"/are/hand:io").c_str());
    //------------------------------------------------------------

    current_class="?";
    true_class="?";

    coding_interrupted=true;
    
    
    blink_init_time=Time::now();
    blink_visible_time=0.5;
    blink_invisible_time=0.0;

    return true;
}

void TransformerThread::run()
{
    mutex.wait();
    Image *img=port_in_img.read(false);
    if(img==NULL)
    {
        mutex.post();
        return;
    }

    Stamp stamp;
    port_in_img.getEnvelope(stamp);

    bool found=false;
    int x,y;
    int pixelCount=0;

    if(mode==MODE_HUMAN_TRACK || mode==MODE_HUMAN_IDLE)
    {
        Bottle *blobs=port_in_blobs.read(false);
        if(blobs!=NULL)
        {
            Bottle *window=blobs->get(0).asList();
            x = window->get(0).asInt();
            y = window->get(1).asInt();
            pixelCount = window->get(2).asInt();
            radius_crop=radius_crop_human;
            found=true;
        }
    }
    
    if(mode==MODE_ROBOT)
    {
        Bottle cmd_are_hand,reply_are_hand;
        cmd_are_hand.addString("get");
        cmd_are_hand.addString("hand");
        cmd_are_hand.addString("image");
        
        port_rpc_are_get_hand.write(cmd_are_hand,reply_are_hand);
        
        if(reply_are_hand.size()>0 && reply_are_hand.get(0).asVocab()!=NACK)
        {
            x=reply_are_hand.get(2).asInt();
            y=reply_are_hand.get(3).asInt();
            pixelCount = -1;
            radius_crop=radius_crop_robot;
            
            if(0<x && x<img->width() && 0<y && y<img->height())
                found=true;
        }
    }
    
    if(found)
    {
        int radius=std::min(radius_crop,x);
        radius=std::min(radius,y);
        radius=std::min(radius,img->width()-x-1);
        radius=std::min(radius,img->height()-y-1);
        
        if(radius>10)
        {
            int radius2=radius<<1;

            img_crop.resize(radius2,radius2);

            cvSetImageROI((IplImage*)img->getIplImage(),cvRect(x-radius,y-radius,radius2,radius2));
            cvCopy((IplImage*)img->getIplImage(),(IplImage*)img_crop.getIplImage());
            cvResetImageROI((IplImage*)img->getIplImage());


            //send the cropped image out and wait for response
            if(!coding_interrupted)
            {
                port_out_crop.setEnvelope(stamp);
                port_out_crop.write(img_crop);

                //////////////////////////////////////////////////////////////////////////////////////////////////////
                port_out_img.setEnvelope(stamp);
                port_out_imginfo.setEnvelope(stamp);

                Bottle imginfo;
                imginfo.addInt(x);
                imginfo.addInt(y);
                imginfo.addInt(pixelCount);
                imginfo.addString(true_class.c_str());

                port_out_imginfo.write(imginfo);
                port_out_img.write(*img);
                //////////////////////////////////////////////////////////////////////////////////////////////////////

            }

            CvFont font;
            cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX,0.8,0.8,0,3);

            int y_text=y-radius-10;
            if(y_text<5) y_text=y+radius+2;
            
            CvScalar text_color=cvScalar(0,0,255);
            string text_string=current_class;
            

            bool blink_visible=true;
            double diff=Time::now()-blink_init_time;
            if(Time::now()-blink_init_time>blink_visible_time)
            {
                if(Time::now()-blink_init_time<blink_visible_time+blink_invisible_time)
                    blink_visible=false;
                else
                    blink_init_time=Time::now();
            }
            
                            
            bool visible=true;

            if(state==STATE_TRAINING)
            {
                text_color=cvScalar(255,0,0);
                text_string="train";

                visible=blink_visible;
            }

            if(state==STATE_OBSERVING)
            {
                text_color=cvScalar(255,0,0);
                text_string="observe: " + current_class;
                
                visible=blink_visible;
            }

            cvRectangle(img->getIplImage(),cvPoint(x-radius,y-radius),cvPoint(x+radius,y+radius),cvScalar(0,255,0),2);

            if(visible)
                cvPutText(img->getIplImage(),text_string.c_str(),cvPoint(x-radius,y_text),&font,text_color);
            
            //cvCircle(img->getIplImage(),cvPoint(x,y),3,cvScalar(255,0,0),3);
        }
    }

    if(port_out_show.getOutputCount()>0)
        port_out_show.write(*img);

    mutex.post();
}

bool TransformerThread::set_current_class(string _current_class)
{
    current_class=_current_class;
    return true;
}

bool TransformerThread::set_true_class(string _true_class)
{
    true_class=_true_class;
    return true;
}

bool TransformerThread::set_mode(int _mode)
{
    mutex.wait();
    mode=_mode;
    mutex.post();
        
    return true;
}

bool TransformerThread::set_state(int _state)
{
    mutex.wait();
    state=_state;
    mutex.post();
        
    return true;
}

bool TransformerThread::get_current_class(string &_current_class)
{
    _current_class=current_class;
    return true;
}

bool TransformerThread::get_true_class(string &_true_class)
{
    _true_class=true_class;
    return true;
}

bool TransformerThread::execReq(const Bottle &command, Bottle &reply)
{
    switch(command.get(0).asVocab())
    {
        default:
            return false;
    }
}

void TransformerThread::interrupt()
{
    mutex.wait();
    port_in_img.interrupt();
    port_in_blobs.interrupt();
    port_out_show.interrupt();
    port_out_crop.interrupt();
                //////////////////////////////////////////////////////////////////////////////////////////////////////
    port_out_img.interrupt();
    port_out_imginfo.interrupt();
                //////////////////////////////////////////////////////////////////////////////////////////////////////
    port_rpc_are_get_hand.interrupt();
    mutex.post();
    cout << "returning transformer thread interrupt..." << endl;
}

void TransformerThread::threadRelease()
{
    mutex.wait();
    port_in_img.close();
    port_in_blobs.close();
    port_out_show.close();
    port_out_crop.close();
                //////////////////////////////////////////////////////////////////////////////////////////////////////
    port_out_img.close();
    port_out_imginfo.close();
                //////////////////////////////////////////////////////////////////////////////////////////////////////
    port_rpc_are_get_hand.close();
    mutex.post();

    cout << "returning transformer thread release..." << endl;
}

bool TransformerThread::interruptCoding()
{
    coding_interrupted=true;
    return true;
}

bool TransformerThread::resumeCoding()
{
    coding_interrupted=false;
    return true;
}
