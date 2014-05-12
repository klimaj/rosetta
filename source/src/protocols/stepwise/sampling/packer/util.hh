// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/sampling/packer/util.hh
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_protocols_stepwise_sampling_packer_util_HH
#define INCLUDED_protocols_stepwise_sampling_packer_util_HH

#include <protocols/stepwise/sampling/packer/StepWisePacker.fwd.hh>
#include <protocols/stepwise/sampling/modeler_options/StepWiseModelerOptions.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/types.hh>

namespace protocols {
namespace stepwise {
namespace sampling {
namespace packer {

	StepWisePackerOP
	get_packer(
		 core::scoring::ScoreFunctionCOP pack_scorefxn,
		 utility::vector1< core::Size > const & moving_res_list,
		 protocols::stepwise::sampling::StepWiseModelerOptionsCOP options );

	utility::vector1< core::Size >
	figure_out_working_interface_res( core::pose::Pose const & pose,
																		utility::vector1< core::Size > const & working_moving_res );

	utility::vector1< core::Size >
	figure_out_working_interface_res( core::pose::Pose const & pose,
																		core::Size const working_moving_res );

	void
 	figure_out_working_interface_res( core::pose::Pose const & pose,
 																		core::Size const working_moving_res,
																		utility::vector1< bool > & interface_res /* save work here */,
																		utility::vector1< utility::vector1< bool > > & checked_pair /* save work here */ );

} //packer
} //sampling
} //stepwise
} //protocols

#endif
