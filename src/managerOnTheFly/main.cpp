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

#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Time.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/RateThread.h>
#include <yarp/os/Semaphore.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/PortReport.h>
#include <yarp/os/Stamp.h>

#include <yarp/sig/Vector.h>
#include <yarp/sig/Image.h>

#include <yarp/math/Math.h>
#include <yarp/math/Rand.h>

#include <highgui.h>
#include <cv.h>

#include <stdio.h>
#include <string>
#include <deque>
#include <algorithm>
#include <vector>
#include <iostream>
#include <fstream>
#include <list>

using namespace std;
using namespace yarp;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;



#define                 ACK                 VOCAB3('a','c','k')
#define                 NACK                VOCAB4('n','a','c','k')

#define                 STATE_IDLE          0
#define                 STATE_TRAINING      1
#define                 STATE_CLASSIFY      2
#define                 STATE_OBSERVING     3

#define                 MODE_ROBOT          0
#define                 MODE_HUMAN_IDLE     1
#define                 MODE_HUMAN_TRACK    2

#define                 CMD_IDLE            VOCAB4('i','d','l','e')

#define                 CMD_OBSERVE         VOCAB4('o','b','s','e')
#define                 CMD_TRAIN           VOCAB4('t','r','a','i')
#define                 CMD_CLASSIFY        VOCAB4('c','l','a','s')

#define                 CMD_ROBOT           VOCAB4('r','o','b','o')
#define                 CMD_HUMAN           VOCAB4('h','u','m','a')

#define                 CMD_FORGET          VOCAB4('f','o','r','g')


class TransformerThread: public RateThread
{
private:
    ResourceFinder                      &rf;
    Semaphore                           mutex;
    bool                                verbose;

    //input
    BufferedPort<Image>                 port_in_img;
    BufferedPort<Bottle>                port_in_blobs;
    BufferedPort<Bottle>				port_in_lh_tactile;
    BufferedPort<Bottle>				port_in_lh_torque;

    //output
    Port                                port_out_show;
    Port                                port_out_crop;
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    Port                                port_out_img;
    Port                                port_out_imginfo;
	Port								port_out_left_hand;
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //rpc
    RpcClient                           port_rpc_are_get_hand;

    int                                 radius_crop; 
    int                                 radius_crop_robot;
    int                                 radius_crop_human;

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    //ImageOf<PixelBgr>                   img_crop;
    ImageOf<PixelRgb>                   img_crop;
    //////////////////////////////////////////////////////////////////////////////////////////////////////

    string                              current_class;
    string                              true_class;

    bool                                coding_interrupted;
    int                                 mode;
    int                                 state;
    
    double                              blink_init_time;
    double                              blink_visible_time;
    double                              blink_invisible_time;

public:
    TransformerThread(ResourceFinder &_rf)
        :RateThread(5),rf(_rf)
    {
    }

    bool threadInit()
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
        port_in_lh_tactile.open(("/"+name+"/lh_tactile:i").c_str());
        port_in_lh_torque.open(("/"+name+"/lh_torque:i").c_str());

        //output
        port_out_show.open(("/"+name+"/show:o").c_str());
        port_out_crop.open(("/"+name+"/crop:o").c_str());
    //////////////////////////////////////////////////////////////////////////////////////////////////////
        port_out_img.open(("/"+name+"/img:o").c_str());
        port_out_imginfo.open(("/"+name+"/imginfo:o").c_str());
        port_out_left_hand.open(("/"+name+"/left_hand:o").c_str());
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

    void run()
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

					if (mode == MODE_ROBOT)
					{
						port_out_left_hand.setEnvelope(stamp);
					}
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

    bool set_current_class(string _current_class)
    {
        current_class=_current_class;
        return true;
    }

    bool set_true_class(string _true_class)
    {
        true_class=_true_class;
        return true;
    }
    
    bool set_mode(int _mode)
    {
        mutex.wait();
        mode=_mode;
        mutex.post();
            
        return true;
    }
    
    bool set_state(int _state)
    {
        mutex.wait();
        state=_state;
        mutex.post();
            
        return true;
    }

    bool get_current_class(string &_current_class)
    {
        _current_class=current_class;
        return true;
    }

    bool get_true_class(string &_true_class)
    {
        _true_class=true_class;
        return true;
    }

    bool execReq(const Bottle &command, Bottle &reply)
    {
        switch(command.get(0).asVocab())
        {
            default:
                return false;
        }
    }

    void interrupt()
    {
        mutex.wait();
        port_in_img.interrupt();
        port_in_blobs.interrupt();
        port_out_show.interrupt();
        port_out_crop.interrupt();
                    //////////////////////////////////////////////////////////////////////////////////////////////////////
        port_out_img.interrupt();
        port_out_imginfo.interrupt();
        port_out_left_hand.interrupt();
                    //////////////////////////////////////////////////////////////////////////////////////////////////////
        port_rpc_are_get_hand.interrupt();
        mutex.post();
        cout << "returning transformer thread interrupt..." << endl;
    }

    void threadRelease()
    {
        mutex.wait();
        port_in_img.close();
        port_in_blobs.close();
        port_out_show.close();
        port_out_crop.close();
                    //////////////////////////////////////////////////////////////////////////////////////////////////////
        port_out_img.close();
        port_out_imginfo.close();
        port_out_left_hand.interrupt();
                    //////////////////////////////////////////////////////////////////////////////////////////////////////
        port_rpc_are_get_hand.close();
        mutex.post();

        cout << "returning transformer thread release..." << endl;
    }

    bool interruptCoding()
    {
        coding_interrupted=true;
        return true;
    }

    bool resumeCoding()
    {
        coding_interrupted=false;
        return true;
    }

};


class StorerThread: public RateThread
{
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

public:
    StorerThread(ResourceFinder &_rf)
        :RateThread(5),rf(_rf)
    {
    }

    bool threadInit()
    {
        verbose=rf.check("verbose");

        string name=rf.find("name").asString().c_str();

        bufferSize = rf.check("BufferSize",Value(50),"Buffer Size").asInt();
        confidence_width=rf.check("confidence_width",Value(500)).asInt();
        confidence_height=rf.check("confidence_height",Value(500)).asInt();

        //Ports
        //-----------------------------------------------------------
        //input
        port_in_scores.open(("/"+name+"/scores:i").c_str());

        //output
        port_out_confidence.open(("/"+name+"/confidence:o").c_str());
        //------------------------------------------------------------

        current_class="?";
//        true_class="?";

        return true;
    }

    void run()
    {
        mutex.wait();
        Bottle *bot = port_in_scores.read(false);

        if(bot==NULL || bot->size()<1)
        {
            mutex.post();
            return;
        }

        string true_class = bot->pop().asString().c_str();
        scores_buffer.push_back(*bot);

        //if the scores exceed a certain threshold clear its head
        while(scores_buffer.size()>bufferSize)
            scores_buffer.pop_front();

        if(scores_buffer.size()<1)
        {
            mutex.post();
            return;
        }

        int n_classes = scores_buffer.front().size();

        vector<double> class_avg(n_classes,0.0);
        vector<int> class_votes(n_classes,0);

        for(list<Bottle>::iterator score_itr=scores_buffer.begin(); score_itr!=scores_buffer.end(); score_itr++)
        {
            double max_score=-1000.0;
            int max_idx;
            for(int class_idx=0; class_idx<n_classes; class_idx++)
            {
                double s=score_itr->get(class_idx).asList()->get(1).asDouble();
                class_avg[class_idx]+=s;
                if(s>max_score)
                {
                    max_score=s;
                    max_idx=class_idx;
                }
            }

            class_votes[max_idx]++;
        }

        double max_avg=-10000.0;
        double max_votes=-10000.0;
        int max_avg_idx;
        int max_votes_idx;
        int max_votes_sum=0;

        for(int class_idx=0; class_idx<n_classes; class_idx++)
        {
            class_avg[class_idx]=class_avg[class_idx]/n_classes;
            if(class_avg[class_idx]>max_avg)
            {
                max_avg=class_avg[class_idx];
                max_avg_idx=class_idx;
            }

            if(class_votes[class_idx]>max_votes)
            {
                max_votes=class_votes[class_idx];
                max_votes_idx=class_idx;
            }
            
            max_votes_sum+=class_votes[class_idx];
        }

        current_class=scores_buffer.front().get(max_avg_idx).asList()->get(0).asString().c_str();
        if(max_votes/scores_buffer.size()<0.2)
            current_class="?";

        cout << "Scores: " << endl;
        for (int i=0; i<n_classes; i++)
            cout << "[" << scores_buffer.front().get(i).asList()->get(0).asString().c_str() << "]: " << class_avg[i] << " "<< class_votes[i] << endl;
        cout << endl << endl;

        //plot confidence values
        if(port_out_confidence.getOutputCount()>0)
        {
            ImageOf<PixelRgb> img_conf;
            img_conf.resize(confidence_width,confidence_height);
            cvZero(img_conf.getIplImage());
            int max_height=(int)img_conf.height()*0.8;
            int min_height=img_conf.height()-20;

            int width_step=(int)img_conf.width()/n_classes;

            for(int class_idx=0; class_idx<n_classes; class_idx++)
            {
                int class_height=img_conf.height()-((int)max_height*class_votes[class_idx]/max_votes_sum);
                if(class_height>min_height)
                    class_height=min_height;

                cvRectangle(img_conf.getIplImage(),cvPoint(class_idx*width_step,class_height),cvPoint((class_idx+1)*width_step,min_height),cvScalar(155,155,255),CV_FILLED);
                cvRectangle(img_conf.getIplImage(),cvPoint(class_idx*width_step,class_height),cvPoint((class_idx+1)*width_step,min_height),cvScalar(0,0,255),3);
                
                CvFont font;
                cvInitFont(&font,CV_FONT_HERSHEY_SIMPLEX,0.6,0.6,0,2);
                
                cvPutText(img_conf.getIplImage(),scores_buffer.front().get(class_idx).asList()->get(0).asString().c_str(),cvPoint(class_idx*width_step,img_conf.height()-5),&font,cvScalar(255,255,255));
            }

            port_out_confidence.write(img_conf);
        }

        mutex.post();
    }


    bool set_current_class(string _current_class)
    {
        mutex.wait();
        current_class=_current_class;
        mutex.post();

        return true;
    }
    bool get_current_class(string &_current_class)
    {
        mutex.wait();
        _current_class=current_class;
        mutex.post();

        return true;
    }

    bool reset_scores()
    {
        mutex.wait();
        scores_buffer.clear();
        mutex.post();

        return true;
    }

    bool set_mode(int _mode)
    {
        mutex.wait();
        mode=_mode;
        mutex.post();
            
        return true;
    }
    bool set_state(int _state)
    {
        mutex.wait();
        state=_state;
        mutex.post();
            
        return true;
    }

    bool execReq(const Bottle &command, Bottle &reply)
    {
        switch(command.get(0).asVocab())
        {
            default:
                return false;
        }
    }

    void interrupt()
    {
        mutex.wait();
        port_in_scores.interrupt();
        port_out_confidence.interrupt();
        mutex.post();
        cout << "returning storer thread interrupt..." << endl;
    }
    void threadRelease()
    {
        mutex.wait();
        port_in_scores.close();
        port_out_confidence.close();
        mutex.post();

        cout << "returning storer thread release..." << endl;
    }
};


class ManagerThread: public RateThread
{
private:
    ResourceFinder                      &rf;
    bool                                verbose;

    Semaphore                           mutex;

    //threads
    TransformerThread                   *thr_transformer;
    StorerThread                        *thr_storer;

    //rpc are
    RpcClient                           port_rpc_are;
    RpcClient                           port_rpc_are_get;
    RpcClient                           port_rpc_are_cmd;
    //rpc human
    RpcClient                           port_rpc_human;
    //rpc classifier
    RpcClient                           port_rpc_classifier;

    //output
    Port                                port_out_speech;

    double                              observe_human_time_training;
    double                              observe_human_time_classify;
    double                              single_operator_time;

    int                                 mode;
    int                                 state;

    deque<string>                       known_objects;
    
    int                                 class_itr_current;
    int                                 class_itr_max;
    double                              reset_label_time;
    double                              curr_time;

private:

    bool set_state(int _state)
    {
        state=_state;
        thr_transformer->set_state(state);
        
        return true;
    }

    bool set_mode(int _mode)
    {
        mode=_mode;
        thr_transformer->set_mode(mode);
        
        return true;
    }

    bool speak(string speech)
    {
        if(port_out_speech.getOutputCount()>0)
        {
            Bottle b;
            b.addString(speech.c_str());
            port_out_speech.write(b);
            return true;
        }
        return false;

    }

    bool observe_robot()
    {
        //check if the robot is already holding an object
        Bottle command,reply;
        command.addString("get");
        command.addString("hold");
        port_rpc_are_get.write(command,reply);

        if(reply.size()==0)
            return false;

        //if the robot is not holding an object then ask the human to give one
        if(reply.get(0).asVocab()!=ACK)
        {
            reply.clear();
            command.clear();
            command.addString("expect");
            command.addString("near");
            command.addString("no_sacc");
            command.addString("left");
            port_rpc_are_cmd.write(command,reply);

            if(reply.size()==0 || reply.get(0).asVocab()!=ACK)
                return false;
        }

        //resume feature storing
        thr_transformer->resumeCoding();

        //perform the exploration of the hand
        reply.clear();
        command.clear();
        command.addString("explore");
        command.addString("hand");
        command.addString("no_sacc");
        port_rpc_are_cmd.write(command,reply);

        //interrupt feature storing
        thr_transformer->interruptCoding();

        return true;
    }

    bool observe_human()
    {

        if (mode==MODE_HUMAN_IDLE)
        {
            // begin tracking code
            Bottle cmd_are,reply_are;

            cmd_are.addString("idle");
            port_rpc_are_cmd.write(cmd_are,reply_are);
            if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
            {
                reply_are.clear();
                cmd_are.clear();
                cmd_are.addString("track");
                cmd_are.addString("motion");
                cmd_are.addString("no_sacc");
                port_rpc_are_cmd.write(cmd_are,reply_are);
                if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
                {
                    set_mode(MODE_HUMAN_TRACK);
                    speak("Begin tracking mode.");
                }
                else
                    speak("Cannot set ARE to 'track motion no_sacc'");
            }
            else
                speak("Cannot set ARE to 'idle'");
            // end tracking code

            // time for the single operator to position the object
            speak("I'm waiting for you to position...");
            Time::delay(single_operator_time);
            speak("*************************** Image acquisition started ***************************");
            thr_transformer->resumeCoding();
        }

        if (state==STATE_OBSERVING)
        {
            Time::delay(observe_human_time_training);
            thr_transformer->interruptCoding();

            // begin tracking code
            Bottle cmd_are,reply_are;

            cmd_are.addString("idle");
            port_rpc_are_cmd.write(cmd_are,reply_are);
            if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
            {
                set_mode(MODE_HUMAN_IDLE);
                speak("End tracking mode.");
            }
            else
                speak("Cannot set ARE to 'idle'");
            // end tracking code

            speak("*************************** Image acquisition done ******************************");
            thr_storer->reset_scores();
        }
        else 
            Time::delay(observe_human_time_classify);

        return true;
    }

    bool observe()
    {
        switch(state)
        {

            case STATE_OBSERVING:
            {
                string current_class;
                thr_transformer->get_current_class(current_class);
                speak("Ok, show me this wonderful " + current_class);

                break;
            }

            case STATE_CLASSIFY:
            {
                speak("Let me see.");
                break;
            }
        }

        bool ok=false;

        if (state==STATE_OBSERVING || state==STATE_CLASSIFY) 
        {
            switch(mode)
            {
                case MODE_ROBOT:
                {
                    ok=observe_robot();
                    break;
                }

                case MODE_HUMAN_IDLE:
                {
                    ok=observe_human();
                    break;
                }

                case MODE_HUMAN_TRACK:
                {
                    ok=observe_human();
                    break;
                }

            }
        }

        return ok;
    }

    bool complete_robot()
    {

        thr_transformer->interruptCoding();

        //just drop the object
        Bottle command,reply;
        command.addString("give");
        port_rpc_are_cmd.write(command,reply);

        command.clear();
        command.addString("home");
        port_rpc_are_cmd.write(command,reply);

        return true;
    }

    bool complete_human()
    {

        thr_transformer->interruptCoding();

        // begin tracking code
        Bottle cmd_are,reply_are;

        cmd_are.addString("idle");
        port_rpc_are_cmd.write(cmd_are,reply_are);
        if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
        {
            set_mode(MODE_HUMAN_IDLE);
            speak("End tracking mode.");
        }
        else
            speak("Cannot set ARE to 'idle'");
        // end tracking code

        return true;
    }

    bool complete()
    {
        bool ok=false;

        switch(mode)
        {
            case MODE_ROBOT:
            {
                ok=complete_robot();
                break;
            }

            case MODE_HUMAN_TRACK:
            {
                ok=complete_human();
                break;
            }

            case MODE_HUMAN_IDLE:
            {
                ok=true;
                break;
            }
        }

        //clear the buffer
        //thr_storer->reset_scores();

        set_state(STATE_IDLE);

        return ok;
    }

    bool classified()
    {
        //do the mumbo jumbo for classification
        //get the buffer from the score store
        string current_class;
        thr_storer->get_current_class(current_class);
        thr_transformer->set_current_class(current_class);

        if(current_class=="?")
        {
            class_itr_current++;
            if(class_itr_current<=class_itr_max)
            {
                speak("I am not sure, let me check it again.");
                return false;
            }
            else
            {
                speak("Sorry, I cannot recognize this object. I am the shame of the whole robot world.");
                return true;
            }
        }
        else
        {
            curr_time=Time::now();
            speak("I think this is a "+current_class);
            return true;
        }
    }

    void decide()
    {
        switch(state)
        {
            case STATE_TRAINING:
            {
                train();
                complete();
                break;
            }

            case STATE_CLASSIFY:
            {
                if(classified())
                    complete();
                break;
            }

            case STATE_OBSERVING:
            {
                complete();
                break;
            }
        }

    }

    bool train()
    {
        bool done=false;
        for(int i=0; !done && i<10; i++)
        {
            Bottle cmd_classifier,reply_classifier;
            cmd_classifier.addString("train");
            port_rpc_classifier.write(cmd_classifier,reply_classifier);

            if(reply_classifier.size()>0 && (reply_classifier.get(0).asVocab()==ACK || reply_classifier.get(0).asString() =="ok"))
                done=true;
        }

        //string current_class;
        //thr_transformer->get_current_class(current_class);
        //speak("Ok, now I know the "+current_class);
        speak("Ok, now I know the observed objects");
        curr_time=Time::now();

        set_state(STATE_IDLE);
        return done;
    }

public:
    ManagerThread(ResourceFinder &_rf)
        :RateThread(10),rf(_rf)
    {
    }

    bool threadInit()
    {
        verbose=rf.check("verbose");

        string name=rf.find("name").asString().c_str();

        observe_human_time_training = rf.check("observe_human_time_training",Value(20.0)).asDouble();
        observe_human_time_classify = rf.check("observe_human_time_classify",Value(15.0)).asDouble();
        single_operator_time = rf.check("single_operator_time",Value(5.0)).asDouble();

        class_itr_max=rf.check("class_iter_max",Value(3)).asInt();

        thr_transformer=new TransformerThread(rf);
        thr_transformer->start();

        thr_storer=new StorerThread(rf);
        thr_storer->start();

        //Ports
        //-----------------------------------------------------------
        //rpc
        port_rpc_are.open(("/"+name+"/are/rpc").c_str());
        port_rpc_are_get.open(("/"+name+"/are/get:io").c_str());
        port_rpc_are_cmd.open(("/"+name+"/are/cmd:io").c_str());
        port_rpc_classifier.open(("/"+name+"/classifier:io").c_str());

        //speech
        port_out_speech.open(("/"+name+"/speech:o").c_str());
        //------------------------------------------------------------

        thr_transformer->interruptCoding();

        set_state(STATE_IDLE);
        set_mode(MODE_HUMAN_IDLE);
        curr_time=Time::now();
        reset_label_time=5.0;
        return true;
    }

    void run()
    {
        if(Time::now()-curr_time > reset_label_time)
        {
            thr_transformer->set_current_class("?");
            curr_time=Time::now();
        }
        if(state==STATE_IDLE)
            return;

        mutex.wait();

        if (state==STATE_OBSERVING || state==STATE_CLASSIFY)
            observe();

        decide();

        mutex.post();
    }

    bool execReq(const Bottle &command, Bottle &reply)
    {
        switch(command.get(0).asVocab())
        {
            default:
                return false;
        }
    }

    bool execHumanCmd(Bottle &command, Bottle &reply)
    {
        switch(command.get(0).asVocab())
        {
            case CMD_IDLE:
            {
                mutex.wait();
                set_state(STATE_IDLE);
                thr_transformer->set_current_class("?");
                thr_transformer->set_true_class("?");
                mutex.post();
                break;
            }

            case CMD_OBSERVE:
            {
                mutex.wait();

                if(command.size()>1)
                {
                    string class_name=command.get(1).asString().c_str();
                    Bottle cmd_classifier,reply_classifier;
                    cmd_classifier.addString("save");
                    cmd_classifier.addString(class_name.c_str());
                    port_rpc_classifier.write(cmd_classifier,reply_classifier);

                    if(reply_classifier.size()>0 && (reply_classifier.get(0).asVocab()==ACK || reply_classifier.get(0).asString() =="ok"))
                    {
                        thr_transformer->set_current_class(class_name);
                        thr_transformer->set_true_class(class_name);
                        set_state(STATE_OBSERVING);

                        bool found=false;
                        for(unsigned int i=0; i<known_objects.size(); i++)
                            if(known_objects[i]==class_name)
                                found=true;

                        if(!found)
                        {
                            known_objects.push_back(class_name);
                            sort(known_objects.begin(),known_objects.end());
                        }

                        reply.addString(("storing " + class_name).c_str());
                    }
                    else
                        reply.addString("classifier busy!");
                }
                else
                    reply.addString("Error! Need to specify a class!");

                mutex.post();
                break;
            }

            case CMD_TRAIN:
            {
                mutex.wait();

                if(command.size()==1)
                {
                    set_state(STATE_TRAINING);
                    reply.addString("learning observed objects");
                }
                else
                    reply.addString("Command not recognized!");

                mutex.post();
                break;
            }

            case CMD_CLASSIFY:
            {
                mutex.wait();

                Bottle cmd_classifier,reply_classifer;
                cmd_classifier.addString("recognize");

                if (command.size()>1) 
                {
                    string class_name = command.get(1).asString().c_str();
                    cmd_classifier.addString(class_name.c_str());
                    port_rpc_classifier.write(cmd_classifier,reply_classifer);

                    if(reply_classifer.size()>0 && (reply_classifer.get(0).asVocab()==ACK || reply_classifer.get(0).asString()=="ok"))
                    {
                        thr_transformer->set_current_class("?");
                        thr_transformer->set_true_class(class_name);

                        if(mode==MODE_ROBOT)
                            thr_storer->reset_scores();
                        set_state(STATE_CLASSIFY);
                        class_itr_current=0;

                        reply.addString(("classifying "+class_name).c_str());
                    }
                    else
                        reply.addString("classifier busy!");

                } else
                {
                    port_rpc_classifier.write(cmd_classifier,reply_classifer);

                    if(reply_classifer.size()>0 && (reply_classifer.get(0).asVocab()==ACK || reply_classifer.get(0).asString()=="ok"))
                    {
                        thr_transformer->set_current_class("?");
                        thr_transformer->set_true_class("?");

                        if(mode==MODE_ROBOT)
                            thr_storer->reset_scores();
                        set_state(STATE_CLASSIFY);
                        class_itr_current=0;

                        reply.addString("classifying");
                    }
                    else
                        reply.addString("classifier busy!");
                }

                mutex.post();
                break;
            }

            case CMD_ROBOT:
            {
                mutex.wait();

                Bottle cmd_are,reply_are;
                cmd_are.addString("idle");
                port_rpc_are_cmd.write(cmd_are,reply_are);

                if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
                {
                    reply_are.clear();
                    cmd_are.clear();
                    cmd_are.addString("home");
                    port_rpc_are_cmd.write(cmd_are,reply_are);

                    if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
                    {
                        set_mode(MODE_ROBOT);
                        reply.addString("ack");
                    }
                    else
                        reply.addString("Error!");
                }
                else
                    reply.addString("Error!");

                mutex.post();
                break;
            }

            case CMD_HUMAN:
            {
                mutex.wait();

                // begin tracking code
                Bottle cmd_are,reply_are;

                cmd_are.addString("idle");
                port_rpc_are_cmd.write(cmd_are,reply_are);
                if(reply_are.size()>0 && reply_are.get(0).asVocab()==ACK)
                {
                    set_mode(MODE_HUMAN_IDLE);
                    reply.addString("ack");
                }
                else
                    reply.addString("Error!");
                // end tracking code

                mutex.post();
                break;
            }

            case CMD_FORGET:
            {
                mutex.wait();
                

                Bottle cmd_classifier,reply_classifer;
                cmd_classifier.addString("forget");
                
                if(command.size()>1)
                {
                    string class_forget=command.get(1).asString().c_str();
                    cmd_classifier.addString(class_forget.c_str());
                
                    port_rpc_classifier.write(cmd_classifier,reply_classifer);
                    
                    if(reply_classifer.size()>0 && reply_classifer.get(0).asVocab()==ACK)
                    {
                        speak("forgotten "+class_forget);
                        reply.addVocab(ACK);
                    }
                    else
                        reply.addVocab(NACK);
                }
                else
                {
                    cmd_classifier.addString("all");
                
                    port_rpc_classifier.write(cmd_classifier,reply_classifer);
                    
                    if(reply_classifer.size()>0 && reply_classifer.get(0).asVocab()==ACK)
                    {
                        speak("I have forgotten all the objects!");
                        reply.addVocab(ACK);
                    }
                    else
                        reply.addVocab(NACK);
                }
                
                
                mutex.post();
                break;
            
            }


        }

        return true;
    }

    void interrupt()
    {
        mutex.wait();

        port_rpc_are.interrupt();
        port_rpc_are_cmd.interrupt();
        port_rpc_are_cmd.interrupt();
        port_rpc_classifier.interrupt();
        port_out_speech.interrupt();

        cout << "calling transformer thread interrupt..." << endl;
        thr_transformer->interrupt();
        cout << "calling storer thread interrupt..." << endl;
        thr_storer->interrupt();

        mutex.post();

        cout << "returning manager thread interrupt..." << endl;
    }

    void threadRelease()
    {
        mutex.wait();
        port_rpc_are.close();
        port_rpc_are_cmd.close();
        port_rpc_are_cmd.close();
        port_rpc_classifier.close();
        port_out_speech.close();

        cout << "calling transformer thread release..." << endl;
        thr_transformer->stop();
        delete thr_transformer;
        cout << "calling storer thread release..." << endl;
        thr_storer->stop();
        delete thr_storer;

        mutex.post();

        cout << "returning manager thread release..." << endl;
    }

};


class ManagerModule: public RFModule
{
protected:
    ManagerThread       *manager_thr;
    RpcServer           port_rpc_human;
    Port                port_rpc;

public:
    ManagerModule()
    {}

    bool configure(ResourceFinder &rf)
    {
        string name=rf.find("name").asString().c_str();

        Time::turboBoost();

        manager_thr=new ManagerThread(rf);
        manager_thr->start();

        port_rpc_human.open(("/"+name+"/human:io").c_str());
        port_rpc.open(("/"+name+"/rpc").c_str());
        attach(port_rpc);
        return true;
    }

    bool interruptModule()
    {
        port_rpc_human.interrupt();
        port_rpc.interrupt();
        cout << "calling manager thread interrupt..." << endl;
        manager_thr->interrupt();

        cout << "returning manager module interrupt..." << endl;
        return true;
    }

    bool close()
    {
        cout << "calling manager thread stop..." << endl;
        manager_thr->stop();
        delete manager_thr;

        port_rpc_human.close();
        port_rpc.close();

        cout << "returning manager module close..." << endl;
        return true;
    }

    bool respond(const Bottle &command, Bottle &reply)
    {
        if(manager_thr->execReq(command,reply))
            return true;
        else
            return RFModule::respond(command,reply);
    }

    double getPeriod()    { return 1.0;  }

    bool   updateModule()
    {
        Bottle human_cmd,reply;
        port_rpc_human.read(human_cmd,true);
        if(human_cmd.size()>0)
        {
            manager_thr->execHumanCmd(human_cmd,reply);
            port_rpc_human.reply(reply);
        }

        return true;
    }

};


int main(int argc, char *argv[])
{
   Network yarp;

   if (!yarp.checkNetwork())
       return -1;

   ResourceFinder rf;
   rf.setVerbose(true);
   rf.setDefaultContext("onthefly-recognition");
   rf.setDefaultConfigFile("config.ini");
   rf.configure(argc,argv);
   rf.setDefault("name","onTheFlyRecognition");
   ManagerModule mod;

   return mod.runModule(rf);
}

