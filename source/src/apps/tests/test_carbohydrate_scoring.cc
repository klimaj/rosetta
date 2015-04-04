// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    apps/tests/test_carbohydrate_scoring.cc
/// @brief   Application source code for testing carbohydrate scoring methods.
/// @author  Labonte  <JWLabonte@jhu.edu>


// Project headers
#include <devel/init.hh>

#include <core/types.hh>
#include <core/chemical/carbohydrates/CarbohydrateInfo.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

#include <protocols/simple_moves/MinMover.hh>

// Utility headers
#include <utility/excn/Exceptions.hh>

// C++ headers
#include <iostream>


using namespace std;
using namespace utility;
using namespace core;
using namespace chemical::carbohydrates;
using namespace pose;
using namespace import_pose;
using namespace scoring;
using namespace protocols;


string const PATH = "input/";


void
output_score( Pose & sugar, core::uint res_num, ScoreFunction const & sf )
{
	cout << " Phi/Psi: " << sugar.phi( res_num ) << '/' << sugar.psi( res_num );
	cout << "  Total Score: " << sf( sugar );
	cout << "  Sugar BB Score: ";
	cout << sugar.energies().residue_total_energies( res_num )[ sugar_bb ] << endl;
}

void
sample_torsions( Pose & sugar, core::uint res_num, ScoreFunction const & sf )
{
	CarbohydrateInfoCOP info( sugar.residue( res_num ).carbohydrate_info() );

	cout << "Residue " << res_num << " is ";
	if ( info->is_alpha_sugar() ) {
		cout << "an alpha sugar." << endl;
	} else {
		cout << "a beta sugar." << endl;
	}
	cout << "Sampling glycosidic bonds for residue " << res_num << "..." << endl;

	for ( Angle phi( -180.0 ); phi <= 180.0; phi += 30.0 ) {
		sugar.set_phi( res_num, phi );

		for ( Angle psi( -180.0 ); psi <= 180.0; psi += 30.0 ) {
			sugar.set_psi( res_num, psi );

			output_score( sugar, res_num, sf );
		}
		cout << endl;
	}
}


int
main( int argc, char *argv[] )
{
	try {
		// Initialize core.
		devel::init( argc, argv );

		// Declare variables.
		Pose maltotriose, lactose;
		ScoreFunctionOP sf( get_score_function() );


		cout << "Importing maltotriose:" << endl;

		pose_from_pdb( maltotriose, PATH + "maltotriose.pdb" );

		sample_torsions( maltotriose, 2, *sf );


		cout << "---------------------------------------------------------------------------------------------" << endl;
		cout << "Importing lactose:" << endl;

		pose_from_pdb( lactose, PATH + "lactose.pdb" );

		sample_torsions( lactose, 2, *sf );

		cout << "Setting lactose on slope toward minimum..." << endl;
		lactose.set_phi( 2, -120.0 );
		lactose.set_psi( 2, 180 );

		output_score( lactose, 2, *sf );

		cout << "Minimizing lactose..." << endl;

		kinematics::MoveMapOP mm( new kinematics::MoveMap );
		mm->set_bb( true );
		mm->set_chi( false );
		mm->set_nu( false );
		simple_moves::MinMover minimizer( mm, sf, "dfpmin", 0.01, true );

		minimizer.apply( lactose );

		output_score( lactose, 2, *sf );

		cout << "Repeating process using the sugar_bb scoring term only..." << endl;

		sf = ScoreFunctionOP( new ScoreFunction );
		sf->set_weight( sugar_bb, 1.0 );
		minimizer.score_function( sf );

		lactose.set_phi( 2, -120.0 );
		lactose.set_psi( 2, 180 );

		output_score( lactose, 2, *sf );

		minimizer.apply( lactose );

		output_score( lactose, 2, *sf );

		cout << "(The minimized angles should be close to -60/120.)" << endl;

		sf->show( cout, lactose );
	} catch ( excn::EXCN_Base const & e ) {
		cerr << "Caught exception: " << e.msg() << endl;
		return -1;
	}
	return 0;
}
