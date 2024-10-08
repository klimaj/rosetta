// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file loopRNA_minimizer.hh
/// @brief
/// @details
///
/// @author Rhiju Das


#ifndef INCLUDED_protocols_stepwise_rna_RNA_AnalyticLoopCloser_HH
#define INCLUDED_protocols_stepwise_rna_RNA_AnalyticLoopCloser_HH

#include <protocols/moves/Mover.hh>
#include <core/id/NamedAtomID.hh>
#include <core/id/DOF_ID.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <utility/fixedsizearray1.fwd.hh>
#include <utility/vector1.hh>
#include <core/types.hh>

//// C++ headers
#include <string>


namespace protocols {
namespace stepwise {
namespace legacy {
namespace modeler {
namespace rna {

/// @brief The RNA de novo structure modeling protocol
class RNA_AnalyticLoopCloser: public protocols::moves::Mover {
public:
	/// @brief Construct the protocol object
	RNA_AnalyticLoopCloser ( core::Size const moving_suite, core::Size const chainbreak_suite );

	/// @brief Clone this object
	protocols::moves::MoverOP clone() const override;

	/// @brief Each derived class must specify its name.  The class name.
	std::string get_name() const override {
		return "RNA_AnalyticLoopCloser";
	}

	/// @brief Apply the loop-rebuild protocol to the input pose
	void apply ( core::pose::Pose & pose ) override;

	void
	choose_best_solution_based_on_score_function ( core::scoring::ScoreFunctionOP scorefxn );

	void
	choose_least_perturb_solution();

	// Undefined, commenting out to fix PyRosetta build  void choose_random_solution();

	void
	get_all_solutions ( core::pose::Pose & pose,
		utility::vector1< core::pose::PoseOP > & pose_list );

	core::Size nsol() {
		return nsol_;
	}

	void
	fill_solution ( core::pose::Pose & pose,
		core::Size const n ) const;

	utility::vector1< core::Real >
	get_torsions ( core::Size const n );

	utility::vector1< utility::vector1< core::Real > >
	get_torsions_for_all_solutions();

private:

	bool
	close_at_cutpoint ( core::pose::Pose & pose );

	void
	figure_out_dof_ids_and_offsets ( core::pose::Pose const & pose,
		utility::vector1< core::Real > const & dt_ang );

	void
	figure_out_offset (
		core::pose::Pose const & pose,
		core::id::DOF_ID const & dof_id,
		core::Real const & original_torsion_value,
		utility::vector1< core::Real > & offset_save );

	void
	apply_solutions ( core::pose::Pose & pose );


	void
	output_chainTORS ( utility::vector1< core::Real > const & dt_ang,
		utility::vector1< core::Real > const & db_ang,
		utility::vector1< core::Real > const & db_len ) const;

	void
	fill_chainTORS (
		core::pose::Pose const & pose,
		utility::vector1< core::id::NamedAtomID > const & atom_ids,
		utility::vector1< utility::fixedsizearray1< core::Real,3 > > & atoms,
		utility::vector1< core::Real > & dt_ang,
		utility::vector1< core::Real > & db_ang,
		utility::vector1< core::Real > & db_len ) const;


private:

	core::Size const moving_suite_;
	core::Size const chainbreak_suite_;

	bool const verbose_;
	int nsol_;

	core::scoring::ScoreFunctionOP scorefxn_;

	utility::vector1< core::id::NamedAtomID > atom_ids_;
	utility::vector1< core::Real > offset_save_;
	utility::vector1< core::id::DOF_ID > dof_ids_;

	utility::vector1< utility::vector1< core::Real > > t_ang_, b_ang_, b_len_;

	bool choose_least_perturb_solution_;
	bool choose_best_solution_;
	//bool choose_random_solution_;

}; // class RNA_AnalyticLoopCloser


} //rna
} //modeler
} //legacy
} //stepwise
} //protocols

#endif
