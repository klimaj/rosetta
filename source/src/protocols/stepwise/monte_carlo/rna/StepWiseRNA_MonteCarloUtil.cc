// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/monte_carlo/rna/StepWiseRNA_MonteCarloUtil.cc
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#include <protocols/stepwise/monte_carlo/rna/StepWiseRNA_MonteCarloUtil.hh>
#include <protocols/stepwise/StepWiseUtil.hh>
#include <core/types.hh>
#include <core/io/silent/SilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/io/silent/BinaryRNASilentStruct.hh>
#include <core/io/silent/util.hh>
#include <core/pose/full_model_info/FullModelInfoUtil.hh>
#include <core/pose/full_model_info/FullModelInfo.hh>
#include <core/scoring/rms_util.hh>

//////////////////////////////////////////////////////////
#include <ObjexxFCL/string.functions.hh>
#include <ObjexxFCL/format.hh>

#include <basic/Tracer.hh>

static basic::Tracer TR( "protocols.stepwise.monte_carlo.rna.StepWiseRNA_MonteCarloUtil" );

using ObjexxFCL::lead_zero_string_of;
using namespace core;

namespace protocols {
namespace stepwise {
namespace monte_carlo {
namespace rna {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// perhaps could use job distributor.

bool
get_out_tag( std::string & out_tag,
						 Size const & n,
						 std::string const & silent_file ){

  using namespace core::io::silent;
	std::map< std::string, bool > tag_is_done;

	static bool init( false );
	if ( !init ){
		tag_is_done = initialize_tag_is_done( silent_file );
		init = true;
	}

	out_tag = "S_"+lead_zero_string_of( n, 6 );
	if (tag_is_done[ out_tag ] ) {
		TR << "Already done: " << out_tag << std::endl;
		return false;
	}
	return true; //ok, not done, so do it.
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
output_to_silent_file( std::string const & out_tag,
											 std::string const & silent_file,
											 pose::Pose & pose,
											 pose::PoseCOP native_pose ){

  using namespace core::io::silent;
  using namespace core::pose::full_model_info;
  using namespace protocols::stepwise;

	// output silent file.
	static SilentFileData const silent_file_data;

	Real rms( 0.0 );
	Real rms_no_bulges ( 0.0 );

	if ( native_pose ) {
		rms = superimpose_at_fixed_res_and_get_all_atom_rmsd( pose, *native_pose );
		rms_no_bulges = superimpose_at_fixed_res_and_get_all_atom_rmsd( pose, *native_pose, true );
	}

	BinaryRNASilentStruct s( pose, out_tag );
	s.add_string_value( "missing", ObjexxFCL::string_of( get_number_missing_residue_connections( pose ) ) );

	if ( native_pose ) {
		s.add_energy( "rms", rms );
		s.add_energy( "non_bulge_rms", rms_no_bulges );
	}

	silent_file_data.write_silent_struct( s, silent_file, false /*score_only*/ );
}


} //rna
} //monte_carlo
} //stepwise
} //protocols
