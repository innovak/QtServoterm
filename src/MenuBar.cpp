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

#include "MenuBar.h"
#include "Actions.h"

namespace STMBL_Servoterm {

MenuBar::MenuBar(Actions *actions, QWidget *parent) : QMenuBar(parent)
{
    QMenu * const fileMenu = addMenu("&File");
    fileMenu->addAction(actions->fileQuit);

    QMenu * const connectionMenu = addMenu("Connection");
    connectionMenu->addAction(actions->connectionConnect);
    connectionMenu->addAction(actions->connectionDisconnect);
    connectionMenu->addSeparator();
    portMenu = connectionMenu->addMenu("Port");
    portGroup = new QActionGroup(this);
    portGroup->setExclusive(true);

    QMenu * const viewMenu = addMenu("&View");
    viewMenu->addAction(actions->viewOscilloscope);
    viewMenu->addAction(actions->viewXYScope);
    viewMenu->addAction(actions->viewConsole);
}

} // namespace STMBL_Servoterm
