// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/devel/denovo_design/GenericSimulatedAnnealer.fwd.hh
/// @brief
/// @author Tom Linsky (tlinsky@uw.edu)

#ifndef INCLUDED_devel_denovo_design_GenericSimulatedAnnealer_fwd_hh
#define INCLUDED_devel_denovo_design_GenericSimulatedAnnealer_fwd_hh


// Utility headers
// AUTO-REMOVED #include <utility/pointer/access_ptr.fwd.hh>
#include <utility/pointer/owning_ptr.fwd.hh>

namespace devel {
namespace denovo_design {

class GenericSimulatedAnnealer;
typedef utility::pointer::owning_ptr< GenericSimulatedAnnealer >  GenericSimulatedAnnealerOP;
typedef utility::pointer::owning_ptr< GenericSimulatedAnnealer const >  GenericSimulatedAnnealerCOP;


} // namespace denovo_design
} // namespace devel

#endif
