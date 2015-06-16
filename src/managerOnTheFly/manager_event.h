#ifndef MANAGER_EVENT_H
#define MANAGER_EVENT_H

/* 
 * Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Bertand HIGY
 * email:  bertrand.higy@iit.it
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

#include <string>

using namespace std;

class ManagerEvent
{
	public:
		enum EventType
		{
			issue_command,
			command_done,
			resume_coding,
			interrupt_coding,
			execute_command
		};
		
		ManagerEvent(EventType type, string details = "");
		string toString() const;
		
	protected:
		static const string names[5];
		
		EventType type_;
		string details_;
};

#endif
