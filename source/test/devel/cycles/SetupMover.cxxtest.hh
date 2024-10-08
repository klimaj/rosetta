// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   test/devel/cycles/SetupMover.cxxtest.hh
/// @brief
/// @author Kale Kundert

#ifndef INCLUDED_devel_cycles_setup_mover_CXXTEST_HH
#define INCLUDED_devel_cycles_setup_mover_CXXTEST_HH

#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

#include <devel/cycles/SetupMover.hh>

#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>

using namespace core;
using namespace devel;

class SetupMoverTests : public CxxTest::TestSuite {

public:

	void setUp() { // {{{1
		core_init();
		import_pose::pose_from_file(pose, "devel/balanced_kic/loop.pdb", core::import_pose::PDB_file);
	}

	void tearDown() { // {{{1
	}
	// }}}1

	void test_something() { // {{{1

	}

	void test_something_else() { // {{{1

	}
	// }}}1

private:

	pose::Pose pose;

};

#endif


