// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   src/core/energy_methods/DNA_DihedralEnergy.hh
/// @brief  dna scoring
/// @author Phil Bradley


#ifndef INCLUDED_core_scoring_methods_DNA_DihedralEnergy_HH
#define INCLUDED_core_scoring_methods_DNA_DihedralEnergy_HH

// Unit headers
#include <core/energy_methods/DNA_DihedralEnergy.fwd.hh>

// Package headers
#include <core/scoring/methods/ContextIndependentLRTwoBodyEnergy.hh>
#include <core/scoring/dna/DNA_DihedralPotential.fwd.hh>


namespace core {
namespace scoring {
namespace methods {

///
class DNA_DihedralEnergy : public ContextIndependentLRTwoBodyEnergy {
public:
	typedef ContextIndependentLRTwoBodyEnergy  parent;
public:

	/// ctor
	DNA_DihedralEnergy();
	DNA_DihedralEnergy( DNA_DihedralEnergy const & src);

	/// clone
	EnergyMethodOP
	clone() const override;

	void
	configure_from_options_system();

	void
	setup_for_scoring( pose::Pose & pose, ScoreFunction const & ) const override;

	bool
	defines_intrares_energy( EnergyMap const & ) const override { return true; }

	bool
	defines_intrares_energy_for_residue(
		conformation::Residue const &
	) const override;

	bool
	defines_residue_pair_energy(
		pose::Pose const & pose,
		Size res1,
		Size res2
	) const override;

	bool
	defines_intrares_dof_derivatives( pose::Pose const & ) const override { return true; }

	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		ScoreFunction const &,
		EnergyMap & emap
	) const override;

	void
	eval_intrares_energy(
		conformation::Residue const &,
		pose::Pose const &,
		ScoreFunction const &,
		EnergyMap &
	) const override;

	///  this function is used for most torsions
	Real
	eval_intraresidue_dof_derivative(
		conformation::Residue const & rsd,
		ResSingleMinimizationData const &, // min_data,
		id::DOF_ID const & dof_id,
		id::TorsionID const & tor_id,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap const & weights
	) const override;


	/// this function is used for the sugar derivs, which dont match up to a "torsion" in the current scheme
	void
	eval_intrares_derivatives(
		conformation::Residue const & rsd,
		ResSingleMinimizationData const & min_data,
		pose::Pose const & pose,
		EnergyMap const & weights,
		utility::vector1< DerivVectorPair > & atom_derivs
	) const override;

	///  this function is used for bb torsions spanning 2 residues
	virtual
	void
	eval_residue_pair_derivatives(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		ResSingleMinimizationData const &,
		ResSingleMinimizationData const &,
		ResPairMinimizationData const & min_data,
		pose::Pose const & pose, // provides context
		EnergyMap const & weights,
		utility::vector1< DerivVectorPair > & r1_atom_derivs,
		utility::vector1< DerivVectorPair > & r2_atom_derivs
	) const override;

	methods::LongRangeEnergyType
	long_range_type() const override;

	core::Size version() const override { return 2; }

	void indicate_required_context_graphs( utility::vector1< bool > & ) const override;

	// data
private:
	dna::DNA_DihedralPotential const & potential_;
	bool score_delta_;
	bool score_chi_;

};

} // methods
} // scoring
} // core


#endif // INCLUDED_core_scoring_EtableEnergy_HH
