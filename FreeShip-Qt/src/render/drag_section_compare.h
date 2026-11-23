// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "auxiliary_geometry.h"

// Display geometry for a single gesture; neither curve belongs to the edited document.
struct DragShellEdge { fs::EntityId a,b;bool symmetric;fs::SectionSpline before,after; bool context=false;QVector<fs::presentation::Cubic> beforeCubics,afterCubics; };
using DragShell=std::shared_ptr<const QVector<DragShellEdge>>;
struct DragSectionPair { fs::Section before,after; };
using DragSections=std::shared_ptr<const QVector<DragSectionPair>>;
