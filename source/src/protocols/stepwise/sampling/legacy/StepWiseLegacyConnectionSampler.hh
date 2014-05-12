// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/stepwise/sampling/legacy/StepWiseLegacyConnectionSampler.hh
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_protocols_stepwise_sampling_StepWiseLegacyConnectionSampler_HH
#define INCLUDED_protocols_stepwise_sampling_StepWiseLegacyConnectionSampler_HH

#include <protocols/moves/Mover.hh>
#include <protocols/stepwise/sampling/modeler_options/StepWiseModelerOptions.fwd.hh>
#include <protocols/stepwise/sampling/working_parameters/StepWiseWorkingParameters.fwd.hh>
#include <protocols/stepwise/sampling/legacy/StepWiseLegacyConnectionSampler.fwd.hh>
#include <protocols/stepwise/sampling/protein/InputStreamWithResidueInfo.fwd.hh>
#include <protocols/stepwise/sampling/rna/checker/RNA_BaseCentroidChecker.fwd.hh>
#include <protocols/stepwise/sampling/rna/checker/RNA_VDW_BinChecker.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <utility/vector1.hh>
#include <core/types.hh>

namespace protocols {
namespace stepwise {
namespace sampling {

	class StepWiseLegacyConnectionSampler: public moves::Mover {

	public:

		//constructor
    StepWiseLegacyConnectionSampler( utility::vector1< core::pose::PoseOP > & pose_list,
															 working_parameters::StepWiseWorkingParametersCOP working_parameters,
															 StepWiseModelerOptionsCOP modeler_options,
															 core::scoring::ScoreFunctionCOP scorefxn);

		//destructor
		~StepWiseLegacyConnectionSampler();

	public:

		virtual void apply( core::pose::Pose & pose );

		virtual std::string get_name() const{ return "StepWiseLegacyConnectionSampler"; }

		void
		do_protein_residue_sampling( core::pose::Pose & pose );

		void
		do_rna_residue_sampling( core::pose::Pose & pose );

		void
		set_input_streams( utility::vector1< protein::InputStreamWithResidueInfoOP > const & input_streams ){ input_streams_ = input_streams; }

		utility::vector1< Size > const & working_obligate_pack_res() const { return working_obligate_pack_res_; }

		void set_skip_sampling( bool const & setting ){ skip_sampling_ = setting; }

		void
		set_pack_scorefxn( core::scoring::ScoreFunctionOP pack_scorefxn ) { pack_scorefxn_ = pack_scorefxn; }

	private:

		utility::vector1< core::pose::PoseOP > & pose_list_; // where work will be saved.
		working_parameters::StepWiseWorkingParametersCOP working_parameters_;
		StepWiseModelerOptionsCOP modeler_options_;
		core::scoring::ScoreFunctionCOP scorefxn_;
		core::scoring::ScoreFunctionOP pack_scorefxn_; // this is actually not in use.

		bool skip_sampling_;

		// setup in swa_protein_main, but not implemented/necessary for stepwise monte carlo.
		utility::vector1< protein::InputStreamWithResidueInfoOP > input_streams_;

		// protein stuff, may be useful
		utility::vector1< core::Size > working_obligate_pack_res_;

	};

} //sampling
} //stepwise
} //protocols

#endif
