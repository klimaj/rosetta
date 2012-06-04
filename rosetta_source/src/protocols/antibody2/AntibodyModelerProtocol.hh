// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer, email:license@u.washington.edu

/// @file protocols/antibody2/AntibodyModelerProtocol.hh
/// @brief Build a homology model of an antibody2
/// @detailed
///
///
/// @author Jianqing Xu ( xubest@gmail.com )


#ifndef INCLUDED_protocols_antibody2_AntibodyModelerProtocol_hh
#define INCLUDED_protocols_antibody2_AntibodyModelerProtocol_hh

#include <utility/vector1.hh>
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/pack/task/TaskFactory.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/moves/PyMolMover.fwd.hh>
#include <protocols/antibody2/AntibodyInfo.hh>
#include <protocols/antibody2/AntibodyModelerProtocol.fwd.hh>




using namespace core;
namespace protocols {
namespace antibody2 {

class AntibodyModelerProtocol: public moves::Mover {
public:

	// default constructor
	AntibodyModelerProtocol();

	// default destructor
	~AntibodyModelerProtocol();

	virtual protocols::moves::MoverOP clone() const;

	/// @brief Assigns default values to primitive members
	void set_default();

	/// @brief Instantiates non-primitive members based on the value of the primitive members
	void sync_objects_with_flags();

	virtual void apply( pose::Pose & pose );

	virtual std::string get_name() const;
    
    /// @brief Associates relevant options with the AntibodyModeler class
    static void register_options();
    
	// simple inline setters
    void set_BenchMark(bool setting) {
        benchmark_ = setting;
    }
    void set_ModelH3(bool setting) {
        model_h3_ = setting; 
    }
	void set_SnugFit(bool setting) {
        snugfit_ = setting;  
    }
    void set_refine_h3(bool setting){
        refine_h3_ = setting;
    }
    void set_H3Filter(bool setting) {
        h3_filter_ = setting;
    }
    void set_CterInsert (bool setting) {
        cter_insert_ = setting;
    }
    void set_sc_min(bool setting) {
        sc_min_ = setting ;
    }
    void set_rt_min(bool setting) {
        rt_min_ = setting ;
    }
    void set_flank_residue_min (bool setting) {
        flank_residue_min_ = setting;
    }
    void set_perturb_type(std::string remodel) {
        h3_perturb_type_ = remodel;
    }
    void set_refine_type (std::string refine)  {
        h3_refine_type_ = refine;
    }
    void set_H3Filter_Tolerance(core::Size const number){
        h3_filter_tolerance_ = number;
    }
    void set_cst_weight ( core::Real const cst_weight){
        cst_weight_ = cst_weight;
    }
    void set_flank_residue_size(core::Real const flank_residue_size) {
        flank_residue_size_ = flank_residue_size;
    }
    void set_middle_pack_min( bool middle_pack_min) {
        middle_pack_min_ = middle_pack_min;
    }


	void display_constraint_residues( pose::Pose & pose );
        
    void show( std::ostream & out=std::cout );
    friend std::ostream & operator<<(std::ostream& out, const AntibodyModelerProtocol & ab_m );
    
    

private:
    bool model_h3_;
    bool snugfit_;
    bool refine_h3_;
    bool h3_filter_;
    bool cter_insert_;
    bool LH_repulsive_ramp_;
    bool sc_min_;
    bool rt_min_;
    bool camelid_;
    bool camelid_constraints_;
    bool flank_residue_min_;
    bool flank_residue_size_;
    bool middle_pack_min_;
    std::string h3_perturb_type_;
    std::string h3_refine_type_;
    core::Real cen_cst_, high_cst_;
    moves::PyMolMoverOP pymol_;
    bool use_pymol_diy_;
    
	// Benchmark mode for shorter_cycles
	bool benchmark_;

	bool user_defined_; // for constructor options passed to init

	bool flags_and_objects_are_in_sync_;
	bool first_apply_with_current_setup_;
    core::Size h3_filter_tolerance_;


	// used as a flag to enable reading in of cst files
	core::Real cst_weight_;

	// score functions    
    core::scoring::ScoreFunctionOP loop_scorefxn_highres_;
    core::scoring::ScoreFunctionOP loop_scorefxn_centroid_;
    core::scoring::ScoreFunctionOP dock_scorefxn_highres_;
    core::scoring::ScoreFunctionOP pack_scorefxn_;
    
	// external objects
	AntibodyInfoOP ab_info_;

	//packer task
	pack::task::TaskFactoryOP tf_;

	/// @brief Assigns user specified values to primitive members using command line options
	void init_from_options();

	/// @brief Performs the portion of setup of non-primitive members that requires a pose - called on apply
	void finalize_setup( core::pose::Pose & pose );

	/// @brief Sets up the instance of AntibodyModeler and initializes all members based on values passed in at construction
	///		or via the command line.
	void init();

	void setup_objects();

}; // class
    
    
    
    
} // namespace antibody2
} // namespace protocols

#endif

