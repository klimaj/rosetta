// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/symmetry/SetupForSequenceSymmetryMover.hh
/// @author Jack Maguire
/// @author Updated by Tim Neary, timdot10@gmail.com


#ifndef INCLUDED_protocols_simple_moves_symmetry_SetupForSequenceSymmetryMover_hh
#define INCLUDED_protocols_simple_moves_symmetry_SetupForSequenceSymmetryMover_hh

// Unit headers
#include <protocols/symmetry/SetupForSequenceSymmetryMover.fwd.hh>
#include <protocols/moves/Mover.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

#include <utility/vector0.hh>

// Utility Headers

namespace protocols {
namespace symmetry {

///////////////////////////////////////////////////////////////////////////////

class SetupForSequenceSymmetryMover : public protocols::moves::Mover
{
public:

	SetupForSequenceSymmetryMover();

	~SetupForSequenceSymmetryMover() override;

	moves::MoverOP clone() const override {
		return utility::pointer::make_shared< SetupForSequenceSymmetryMover >( *this );
	}

	void apply( core::pose::Pose & pose ) override;

	void parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap &data
	) override;

	std::string
	get_name() const override {
		return "SetupForSequenceSymmetryMover";
	}

	static
	std::string
	mover_name() {
		return "SetupForSequenceSymmetryMover";
	}

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	void
	add_residue_selector( core::Size region, core::select::residue_selector::ResidueSelectorCOP const & selector );

	void
	set_prefix_name( std::string const & setting ) { setup_magic_name_prefix_ = setting; }

private: // Member methods

	void
	validate_residue_selectors( core::pose::Pose const & pose,
		utility::vector0< core::select::residue_selector::ResidueSelectorCOP > const & res_seles ) const;

private: // Member variables
	std::string setup_magic_name_prefix_;
	utility::vector0< utility::vector0< core::select::residue_selector::ResidueSelectorCOP > > independent_regions_;

};


}
} // rosetta
#endif
