/*
* This file is part of the stmbl project.
*
* Copyright (C) 2020 Forest Darling <fdarling@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Actions.h"

namespace STMBL_Servoterm {

Actions::Actions(QObject *parent) : QObject(parent)
{
    fileQuit = new QAction("&Quit", this);
    serialConnect = new QAction("Connect", this);
    serialDisconnect = new QAction("Disconnect", this);
    viewOscilloscope = new QAction("Show Oscilloscope", this);
    viewXYScope = new QAction("Show X/Y Scope", this);
    viewConsole = new QAction("Show Console Output", this); // TODO change this to "Show Console"
    viewOscilloscope->setCheckable(true);
    viewXYScope->setCheckable(true);
    viewConsole->setCheckable(true);
}

} // namespace STMBL_Servoterm
