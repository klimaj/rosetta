// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/protocols/moves/MonteCarloTest.cc
/// @author Sarel Fleishman (sarelf@uw.edu)


// Unit Headers
#include <protocols/moves/MonteCarloTest.hh>
#include <protocols/moves/MonteCarloTestCreator.hh>
#include <protocols/moves/GenericMonteCarloMover.hh>

// Package Headers

// Project Headers
#include <core/pose/Pose.hh>
#include <basic/Tracer.hh>
// AUTO-REMOVED #include <protocols/filters/Filter.hh>
#include <protocols/moves/Mover.hh>

// Parser headers
#include <protocols/moves/DataMap.fwd.hh>
#include <utility/tag/Tag.hh>

#include <utility/vector0.hh>
#include <utility/vector1.hh>


// Utility headers

//// C++ headers

static basic::Tracer TR("protocols.moves.MonteCarloTest");

using namespace core;

namespace protocols {
namespace moves {

std::string
MonteCarloTestCreator::keyname() const
{
	return MonteCarloTestCreator::mover_name();
}

protocols::moves::MoverOP
MonteCarloTestCreator::create_mover() const {
	return new MonteCarloTest;
}

std::string
MonteCarloTestCreator::mover_name()
{
	return "MonteCarloTest";
}

std::string
MonteCarloTest::get_name() const {
	  return MonteCarloTestCreator::mover_name();
}


/// @brief default constructor
MonteCarloTest::MonteCarloTest():
	Mover("MonteCarloTest"),
	MC_mover_( NULL )
{
}

/// @brief destructor
MonteCarloTest::~MonteCarloTest(){}

/// @brief clone this object
MoverOP
MonteCarloTest::clone() const
{
	return new MonteCarloTest( *this );
}

/// @brief create this type of object
MoverOP
MonteCarloTest::fresh_instance() const
{
	return new MonteCarloTest();
}

GenericMonteCarloMoverOP
MonteCarloTest::get_MC() const{
	return( MC_mover_ );
}

void
MonteCarloTest::set_MC( GenericMonteCarloMoverOP mc ){
	MC_mover_ = mc;
}

void
MonteCarloTest::parse_my_tag( TagPtr const tag, DataMap &, Filters_map const &, Movers_map const &movers, Pose const & pose ){
	std::string const mc_name( tag->getOption< std::string >( "MC_name" ) );
	Movers_map::const_iterator find_mover( movers.find( mc_name ) );
	if( find_mover == movers.end() )
		utility_exit_with_message( "MC mover not found by MonteCarloTest" );

	set_MC( dynamic_cast< GenericMonteCarloMover * >( find_mover->second() ) );
	Pose temp_pose( pose );
	get_MC()->initialize();
	get_MC()->reset( temp_pose );
	TR<<"Setting MonteCarlo container with mover "<<mc_name<<std::endl;
}

void
MonteCarloTest::apply( core::pose::Pose & pose ){
	bool const accept( MC_mover_->boltzmann( pose ) );
	TR<<"MC mover accept="<<accept<<std::endl;
}

} // ns moves
} // ns protocols
