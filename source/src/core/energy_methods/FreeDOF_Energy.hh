// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/energy_methods/FreeDOF_Energy.hh
/// @brief  Score function class
/// @author Rhiju Das (rhiju@stanford.edu)


#ifndef INCLUDED_core_energy_methods_FreeDOF_Energy_hh
#define INCLUDED_core_energy_methods_FreeDOF_Energy_hh

// Unit headers
#include <core/energy_methods/FreeDOF_Energy.fwd.hh>
#include <core/scoring/methods/FreeDOF_Options.fwd.hh>

// Package headers
#include <core/scoring/methods/ContextIndependentOneBodyEnergy.hh>
#include <core/scoring/methods/EnergyMethodOptions.hh>
#include <core/conformation/Residue.fwd.hh>

// Project headers
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>

#include <utility/vector1.hh>


namespace core {
namespace energy_methods {



class FreeDOF_Energy : public core::scoring::methods::ContextIndependentOneBodyEnergy  {
public:
	typedef core::scoring::methods::ContextIndependentOneBodyEnergy  parent;

public:

	/// @brief ctor
	FreeDOF_Energy( core::scoring::methods::EnergyMethodOptions const & energy_method_options );

	/// @brief dtor
	~FreeDOF_Energy() override;

	/// clone
	core::scoring::methods::EnergyMethodOP
	clone() const override;


	void
	setup_for_scoring( pose::Pose & pose, core::scoring::ScoreFunction const & ) const override;

	/////////////////////////////////////////////////////////////////////////////
	// methods for ContextIndependentOneBodyEnergies
	/////////////////////////////////////////////////////////////////////////////


	void
	residue_energy(
		conformation::Residue const & rsd,
		pose::Pose const &,
		core::scoring::EnergyMap & emap
	) const override;


	/// @brief FreeDOF_Energy is context independent; indicates that no
	/// context graphs are required
	void indicate_required_context_graphs( utility::vector1< bool > & ) const override;

	core::Size version() const override;

	void
	finalize_total_energy(
		pose::Pose & pose,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap & totals ) const override;

private:


	void
	accumulate_stack_energy(
		pose::Pose & pose,
		core::scoring::ScoreFunction const & scorefxn,
		utility::vector1< Real > & stack_energy ) const;

	void
	do_fa_stack_scorefunction_checks( core::scoring::ScoreFunction const & scorefxn ) const;

	void
	get_hbond_energy(
		pose::Pose & pose,
		core::scoring::ScoreFunction const & scorefxn,
		utility::vector1< Real > & base_hbond_energy,
		utility::vector1< Real > & sugar_hbond_energy ) const;

private:

	core::scoring::methods::EnergyMethodOptions const & energy_method_options_;
	core::scoring::methods::FreeDOF_Options const & options_;
	utility::vector1< Real > const & free_res_weights_;

};

} // scoring
} // core


#endif // INCLUDED_core_scoring_EtableEnergy_HH
