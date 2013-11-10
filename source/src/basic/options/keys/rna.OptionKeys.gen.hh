// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   basic/options/keys/rna.OptionKeys.gen.hh
/// @brief  basic::options::OptionKeys collection
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)
/// @author James M. Thompson (tex@u.washington.edu)

#ifndef INCLUDED_basic_options_keys_rna_OptionKeys_gen_HH
#define INCLUDED_basic_options_keys_rna_OptionKeys_gen_HH

// Unit headers
#include <basic/options/keys/OptionKeys.hh>

namespace basic {
namespace options {
namespace OptionKeys {

namespace rna { extern BooleanOptionKey const rna; }
namespace rna { extern IntegerOptionKey const minimize_rounds; }
namespace rna { extern BooleanOptionKey const corrected_geo; }
namespace rna { extern BooleanOptionKey const vary_geometry; }
namespace rna { extern BooleanOptionKey const skip_coord_constraints; }
namespace rna { extern BooleanOptionKey const skip_o2prime_trials; }
namespace rna { extern StringOptionKey const vall_torsions; }
namespace rna { extern BooleanOptionKey const jump_database; }
namespace rna { extern BooleanOptionKey const bps_database; }
namespace rna { extern BooleanOptionKey const rna_prot_erraser; }
namespace rna { extern BooleanOptionKey const deriv_check; }

} // namespace OptionKeys
} // namespace options
} // namespace basic

#endif
