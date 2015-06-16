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

#ifndef MANAGER_THREAD_H
#define MANAGER_THREAD_H

#include <deque>
#include <string>

#include <yarp/os/Bottle.h>
#include <yarp/os/Port.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/Semaphore.h>
#include <yarp/os/Stamp.h>

#include "storer_thread.h"
#include "transformer_thread.h"
#include "manager_event.h"

using namespace std;
using namespace yarp::os;

class ManagerThread: public RateThread
{
	public:
		enum Event { EXPECT_START, EXPECT_DONE, EXPLORE_START, EXPLORE_DONE, GIVE_START, GIVE_DONE, HOME_START, HOME_DONE };
	
		ManagerThread(ResourceFinder &_rf);
		bool threadInit();
		void run();
		bool execReq(const Bottle &command, Bottle &reply);
		bool execHumanCmd(Bottle &command, Bottle &reply);
		void interrupt();
		void threadRelease();

	private:
		ResourceFinder      	&rf;
		bool                	verbose;

		Semaphore           	mutex;

		//threads
		TransformerThread   	*thr_transformer;
		StorerThread    		*thr_storer;

		//rpc are
		RpcClient       		port_rpc_are;
		RpcClient       		port_rpc_are_get;
		RpcClient      			port_rpc_are_cmd;
		//rpc human
		RpcClient       		port_rpc_human;
		//rpc classifier
		RpcClient       		port_rpc_classifier;
		
		// input
		BufferedPort<Bottle>	port_in_timestamp;

		//output
		Port            		port_out_speech;
		Port					port_out_events;

		double          		observe_human_time_training;
		double          		observe_human_time_classify;
		double          		single_operator_time;

		int						mode;
		int						state;

		deque<string>			known_objects;
		
		int						class_itr_current;
		int						class_itr_max;
		double					reset_label_time;
		double					curr_time;

		bool set_state(int _state);
		bool set_mode(int _mode);
		bool speak(string speech);
		bool observe_robot();
		bool observe_human();
		bool observe();
		bool complete_robot();
		bool complete_human();
		bool complete();
		bool classified();
		void decide();
		bool train();
		void fireEvent(const ManagerEvent &e);
		void issueAreCmd(Bottle &command, Bottle &reply);
		void resumeCoding();
		void interruptCoding();
};

#endif
