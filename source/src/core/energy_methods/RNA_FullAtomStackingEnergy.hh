// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/rna/RNA_FullAtomStacking.hh
/// @brief  Statistically derived rotamer pair potential class declaration
/// @author Rhiju Das

#ifndef INCLUDED_core_scoring_rna_RNA_FullAtomStackingEnergy_HH
#define INCLUDED_core_scoring_rna_RNA_FullAtomStackingEnergy_HH

// Unit Headers
#include <core/energy_methods/RNA_FullAtomStackingEnergy.fwd.hh>

// Package headers
#include <core/scoring/methods/ContextDependentTwoBodyEnergy.hh>

// Project headers
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/etable/count_pair/CountPairFunction.fwd.hh>
#include <core/scoring/methods/EnergyMethodOptions.fwd.hh>

//Auto Headers
#include <utility/vector1.hh>
#include <numeric/xyzMatrix.fwd.hh>


namespace core {
namespace energy_methods {

typedef  numeric::xyzMatrix< Real > Matrix;


class RNA_FullAtomStackingEnergy : public core::scoring::methods::ContextDependentTwoBodyEnergy  {
public:
	typedef core::scoring::methods::ContextDependentTwoBodyEnergy  parent;

public:

	RNA_FullAtomStackingEnergy( core::scoring::methods::EnergyMethodOptions const & options );

	RNA_FullAtomStackingEnergy( RNA_FullAtomStackingEnergy const & src );

	/// clone
	core::scoring::methods::EnergyMethodOP
	clone() const override;

	/////////////////////////////////////////////////////////////////////////////
	// scoring
	/////////////////////////////////////////////////////////////////////////////

	void
	setup_for_scoring( pose::Pose & pose, core::scoring::ScoreFunction const & scfxn ) const override;

	void
	setup_for_minimizing(
		pose::Pose & pose,
		core::scoring::ScoreFunction const & sfxn,
		kinematics::MinimizerMapBase const & min_map
	) const override;

	void
	setup_for_derivatives( pose::Pose & pose, core::scoring::ScoreFunction const & scfxn ) const override;

	void
	setup_for_packing( pose::Pose & , utility::vector1< bool > const &, utility::vector1< bool > const & ) const override {}

	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap & emap
	) const override;

	void
	eval_intrares_energy(
		conformation::Residue const &,
		pose::Pose const &,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap &
	) const override {}


	void
	eval_atom_derivative(
		id::AtomID const & atom_id,
		pose::Pose const & pose,
		kinematics::DomainMap const & domain_map,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap const & weights,
		Vector & F1,
		Vector & F2
	) const override;

	bool
	defines_intrares_energy( core::scoring::EnergyMap const & /*weights*/ ) const override { return false; }

	void
	finalize_total_energy(
		pose::Pose & pose,
		core::scoring::ScoreFunction const &,
		core::scoring::EnergyMap &// totals
	) const override;

	Distance
	atomic_interaction_cutoff() const override;

	void indicate_required_context_graphs( utility::vector1< bool > & ) const override {}

	/// @brief Interface function for class core::scoring::NeighborList.
	core::scoring::etable::count_pair::CountPairFunctionCOP
	get_intrares_countpair(
		conformation::Residue const &,
		pose::Pose const &,
		core::scoring::ScoreFunction const &
	) const;

	/// @brief Interface function for class core::scoring::NeighborList.
	core::scoring::etable::count_pair::CountPairFunctionCOP
	get_count_pair_function(
		Size const,
		Size const,
		pose::Pose const &,
		core::scoring::ScoreFunction const &
	) const;

	core::scoring::etable::count_pair::CountPairFunctionCOP
	get_count_pair_function(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2
	) const;

	/////////////////////////////////////////////////////////////////////////////
	// data
	/////////////////////////////////////////////////////////////////////////////

private:

	// Real
	// get_fa_stack_score( Distance const dist, Real const cos_kappa ) const;

	Real
	get_fa_stack_score( Vector const & r_vec, Matrix const & M_i,
		Real const prefactor,
		Distance const stack_cutoff, Distance const dist_cutoff) const;

	Vector
	get_fa_stack_deriv(
		Vector const & r_vec,
		Matrix const & M_i,
		Real const prefactor,
		Distance const stack_cutoff,
		Distance const dist_cutoff ) const;

	Real
	residue_pair_energy_one_way(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		Real & score_aro,
		Real     const & prefactor,
		Distance const & stack_cutoff,
		Distance const & dist_cutoff
	) const;

	virtual
	void
	eval_atom_derivative(
		id::AtomID const & atom_id,
		pose::Pose const & pose,
		kinematics::DomainMap const & domain_map,
		Vector & F1,
		Vector & F2,
		Real const fa_stack_weight,
		Real const fa_stack_lower_weight,
		Real const fa_stack_upper_weight,
		Real const fa_stack_aro_weight,
		Real const prefactor,
		Distance const stack_cutoff,
		Distance const dist_cutoff
	) const;

	bool
	check_base_base_OK(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		Size const m, Size const n ) const;

	core::Size version() const override;

	Real const prefactor_;
	Distance const stack_cutoff_;
	Distance const dist_cutoff_;

	Real const sol_prefactor_;
	Distance const sol_stack_cutoff_;
	Distance const sol_dist_cutoff_;

	Real const lr_prefactor_;
	Distance const lr_stack_cutoff_;
	Distance const lr_dist_cutoff_;

	bool const base_base_only_;
	bool const include_rna_prot_;

};


} //scoring
} //core

#endif // INCLUDED_core_scoring_ScoreFunction_HH
