// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/sampling/align/util.hh
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_protocols_stepwise_sampling_align_util_HH
#define INCLUDED_protocols_stepwise_sampling_align_util_HH

#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>

namespace protocols {
namespace stepwise {
namespace sampling {
namespace align {

	core::Real
	get_rmsd( core::pose::Pose const & pose1, core::pose::Pose const & pose2,
						utility::vector1< core::Size > const & calc_rms_res,
						bool const check_align_at_superimpose_res = false,
						bool const check_switch = false );

	core::Real
	get_rmsd( core::pose::Pose const & pose1, core::pose::Pose const & pose2,
						bool const check_align_at_superimpose_res = false,
						bool const check_switch = false );

} //align
} //sampling
} //stepwise
} //protocols

#endif
