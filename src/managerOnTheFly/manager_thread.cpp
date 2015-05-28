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

#include <yarp/os/ResourceFinder.h>
#include <yarp/os/Time.h>

#include "constants.h"
#include "manager_thread.h"

ManagerThread::ManagerThread(ResourceFinder &_rf): RateThread(10),rf(_rf)
{
}

bool ManagerThread::threadInit()
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

void ManagerThread::run()
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

bool ManagerThread::execReq(const Bottle &command, Bottle &reply)
{
    switch(command.get(0).asVocab())
    {
        default:
            return false;
    }
}

bool ManagerThread::execHumanCmd(Bottle &command, Bottle &reply)
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

void ManagerThread::interrupt()
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

void ManagerThread::threadRelease()
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

bool ManagerThread::set_state(int _state)
{
    state=_state;
    thr_transformer->set_state(state);
    
    return true;
}

bool ManagerThread::set_mode(int _mode)
{
    mode=_mode;
    thr_transformer->set_mode(mode);
    
    return true;
}

bool ManagerThread::speak(string speech)
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

bool ManagerThread::observe_robot()
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

bool ManagerThread::observe_human()
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

bool ManagerThread::observe()
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

bool ManagerThread::complete_robot()
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

bool ManagerThread::complete_human()
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

bool ManagerThread::complete()
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

bool ManagerThread::classified()
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

void ManagerThread::decide()
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

bool ManagerThread::train()
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
