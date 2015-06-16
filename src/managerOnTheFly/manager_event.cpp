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

#include "manager_event.h"

const string ManagerEvent::names[] = {"issue_command", "command_done", "resume_coding", "interrupt_coding", "execute_command"};
		

ManagerEvent::ManagerEvent(EventType type, string details): type_(type), details_(details)
{
}

string ManagerEvent::toString() const
{
	string s;
	if (details_ == "")
		s = names[type_];
	else
		s = names[type_] + " " + details_;
	return s;
}
