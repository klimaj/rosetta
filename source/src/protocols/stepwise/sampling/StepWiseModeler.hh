// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/sampling/StepWiseModeler.hh
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_protocols_stepwise_sampling_StepWiseModeler_HH
#define INCLUDED_protocols_stepwise_sampling_StepWiseModeler_HH

#include <protocols/moves/Mover.hh>
#include <protocols/stepwise/sampling/StepWiseModeler.hh>
#include <protocols/stepwise/sampling/StepWiseModeler.fwd.hh>
#include <protocols/stepwise/sampling/modeler_options/StepWiseModelerOptions.hh>
#include <protocols/stepwise/sampling/packer/StepWiseMasterPacker.fwd.hh>
#include <protocols/stepwise/sampling/working_parameters/StepWiseWorkingParameters.fwd.hh>
#include <protocols/stepwise/sampling/protein/InputStreamWithResidueInfo.fwd.hh>
#include <protocols/toolbox/AllowInsert.fwd.hh>
#include <core/kinematics/MoveMap.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/pose/Pose.fwd.hh>

namespace protocols {
namespace stepwise {
namespace sampling {

	class StepWiseModeler: public moves::Mover {

	public:

		//Constructor
		StepWiseModeler( Size const moving_res, core::scoring::ScoreFunctionCOP scorefxn );

		//constructor
		StepWiseModeler( core::scoring::ScoreFunctionCOP scorefxn );

		//destructor
		~StepWiseModeler();

		StepWiseModeler( StepWiseModeler const & src );

	public:

		virtual StepWiseModelerOP clone_modeler() const;

		StepWiseModeler & operator=( StepWiseModeler const & src );

		virtual void apply( core::pose::Pose & pose );

		virtual std::string get_name() const{ return "StepWiseModeler"; }

		void set_moving_res_and_reset( Size const moving_res );

		void set_working_prepack_res( utility::vector1< Size > const & setting ){
			working_prepack_res_ = setting;
			prepack_res_was_inputted_ = true;
		}

		void set_working_minimize_res( utility::vector1< Size > const & setting ){ working_minimize_res_ = setting; }

		void set_figure_out_prepack_res( bool const setting ){ figure_out_prepack_res_ = setting; }

		void set_working_parameters( working_parameters::StepWiseWorkingParametersCOP working_parameters );
		working_parameters::StepWiseWorkingParametersCOP working_parameters();

		void set_modeler_options( StepWiseModelerOptionsCOP options ) { modeler_options_ = options; }

		Size
		get_num_sampled(){ return pose_list_.size(); }

		void
		set_input_streams( utility::vector1< protein::InputStreamWithResidueInfoOP > const & input_streams );

		void set_moving_res_list( utility::vector1< Size > const & setting ){ moving_res_list_ = setting; }

	private:

		void
		initialize( pose::Pose & pose );

		void
		reinitialize( pose::Pose & pose );

		void
		initialize_working_parameters_and_root( core::pose::Pose & pose );

		void
		initialize_scorefunctions( pose::Pose const & pose );

		void
		do_prepacking( core::pose::Pose & pose );

		void
		do_sampling( core::pose::Pose & pose );

		void
		do_minimizing( core::pose::Pose & pose );

		void figure_out_moving_res_list( core::pose::Pose const & pose );

		void
		figure_out_moving_res_list_from_most_distal_res( core::pose::Pose const & pose, Size const moving_res );

		bool
		sampling_successful();

	private:

		Size moving_res_;
		utility::vector1< Size > moving_res_list_;
		utility::vector1< Size > working_minimize_res_;
		utility::vector1< Size > working_prepack_res_;

		working_parameters::StepWiseWorkingParametersCOP working_parameters_;
		StepWiseModelerOptionsCOP modeler_options_;
		scoring::ScoreFunctionCOP scorefxn_;
		scoring::ScoreFunctionCOP sample_scorefxn_, pack_scorefxn_; // will be derived from scorefxn_

		// setup in swa_protein_main, but not implemented/necessary for stepwise monte carlo.
		utility::vector1< protein::InputStreamWithResidueInfoOP > input_streams_;

		utility::vector1< core::pose::PoseOP > pose_list_;

		bool figure_out_prepack_res_;
		bool prepack_res_was_inputted_;
		packer::StepWiseMasterPackerOP master_packer_;

	};

} //sampling
} //stepwise
} //protocols

#endif
