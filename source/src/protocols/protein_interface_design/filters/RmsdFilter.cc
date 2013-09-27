// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/protein_interface_design/filters/RmsdFilter.cc
/// @brief rmsd filtering
/// @author Jacob Corn (jecorn@u.washington.edu)
#include <protocols/protein_interface_design/filters/RmsdFilter.hh>
#include <protocols/protein_interface_design/filters/RmsdFilterCreator.hh>
#include <protocols/filters/Filter.hh>

// Project Headers
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/pose/datacache/cacheable_observers.hh>
#include <core/conformation/Conformation.hh>
#include <utility/tag/Tag.hh>
// AUTO-REMOVED #include <protocols/moves/DataMap.hh>
#include <protocols/moves/Mover.fwd.hh> //Movers_map
#include <core/pose/PDBInfo.hh>

#include <core/scoring/rms_util.hh>
#include <core/scoring/rms_util.tmpl.hh>
#include <numeric/model_quality/rms.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <core/pose/selection.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
// AUTO-REMOVED #include <core/io/pdb/pose_io.hh>
#include <core/import_pose/import_pose.hh>

#include <core/id/AtomID.hh>
#include <core/id/AtomID_Map.hh>
#include <core/id/NamedAtomID.hh>
#include <ObjexxFCL/FArray1D.hh>
#include <ObjexxFCL/FArray2D.hh>

#include <algorithm>
#include <list>

#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <basic/Tracer.hh>

namespace protocols {
namespace protein_interface_design {
namespace filters {

using namespace ObjexxFCL;

RmsdFilter::RmsdFilter() :
	protocols::filters::Filter( "Rmsd" ),
	superimpose_( true ),
	symmetry_( false ),
	threshold_( 5.0 ),
	reference_pose_( NULL ),
	selection_from_segment_cache_(false),
	superimpose_on_all_( false ),
  specify_both_spans_( false ),
  CA_only_ ( true ),
  begin_native_ (0),
  end_native_ (0),
  begin_pose_ (0),
  end_pose_ ( 0 )
{
	selection_.clear();
}

RmsdFilter::RmsdFilter(
	std::list<core::Size> const selection,
	bool const superimpose,
	core::Real const threshold,
	core::pose::PoseOP reference_pose
) : protocols::filters::Filter( "Rmsd" ),
		selection_(selection),
		superimpose_(superimpose),
		threshold_(threshold),
		reference_pose_(reference_pose),
		selection_from_segment_cache_(false),
    specify_both_spans_( false ),
    CA_only_ ( true ),
    begin_native_ (0),
    end_native_ (0),
    begin_pose_ (0),
    end_pose_ ( 0 )
{}


RmsdFilter::~RmsdFilter() {}

protocols::filters::FilterOP
RmsdFilter::clone() const {
	return new RmsdFilter( *this );
}

static basic::Tracer TR( "protocols.protein_interface_design.filters.RmsdFilter" );
core::Real
RmsdFilter::compute( core::pose::Pose const & pose ) const
{
	using namespace core;
	using namespace core::scoring;
	core::pose::Pose copy_pose = pose;
	core::pose::Pose native = *reference_pose_;
	core::Real rmsd( 0.0 );
  if ( specify_both_spans_ ) {
		if (CA_only_) {
			std::map<core::Size, core::Size> correspondence;
	    for (core::Size i=0; i<end_native_-begin_native_; ++i)
				correspondence[begin_native_+i]=begin_pose_+i;
			rmsd=core::scoring::CA_rmsd( native, copy_pose, correspondence);
	} else {
      FArray2D_double pose_coordinates_;
      FArray2D_double ref_coordinates_;
      pose_coordinates_.redimension(3, 4*(end_native_-begin_native_), 0.0);
      ref_coordinates_.redimension(3, 4*(end_native_-begin_native_), 0.0);
  
      core::Size n_at = 1;
      for (core::Size i = 0; i < end_native_-begin_native_; ++i) {
  
        for (core::Size d = 1; d <= 3; ++d) {
          pose_coordinates_(d, n_at) = copy_pose.xyz(core::id::NamedAtomID("N", begin_pose_+i))[d - 1];
          ref_coordinates_(d, n_at) = native.xyz(core::id::NamedAtomID("N", begin_native_+i))[d - 1];
        }
        n_at++;
  
        for (core::Size d = 1; d <= 3; ++d) {
          pose_coordinates_(d, n_at) = copy_pose.xyz(core::id::NamedAtomID("CA", begin_pose_+i))[d - 1];
          ref_coordinates_(d, n_at) = native.xyz(core::id::NamedAtomID("CA", begin_native_+i))[d - 1];
        }
        n_at++;
  
        for (core::Size d = 1; d <= 3; ++d) {
          pose_coordinates_(d, n_at) = copy_pose.xyz(core::id::NamedAtomID("C", begin_pose_+i))[d - 1];
          ref_coordinates_(d, n_at) = native.xyz(core::id::NamedAtomID("C", begin_native_+i))[d - 1];
        }
        n_at++;
  
        for (core::Size d = 1; d <= 3; ++d) {
          pose_coordinates_(d, n_at) = copy_pose.xyz(core::id::NamedAtomID("O", begin_pose_+i))[d - 1];
          ref_coordinates_(d, n_at) = native.xyz(core::id::NamedAtomID("O", begin_native_+i))[d - 1];
        }
        n_at++;
      }
 		 rmsd = numeric::model_quality::rms_wrapper(4*(end_native_-begin_native_), pose_coordinates_, ref_coordinates_);
  }

		return rmsd;
	}

	if ( !symmetry_ )
		runtime_assert_msg( copy_pose.total_residue() == native.total_residue(), "the reference pose must be the same size as the working pose" );

	// generate temporary FArray
	FArray1D_bool selection_array( pose.total_residue(), false ); // on which residues to check rmsd
	FArray1D_bool superimpose_array( pose.total_residue(), false ); // which residues to superimpose

	if( selection_from_segment_cache_ ) 
		core::pose::datacache::SpecialSegmentsObserver::set_farray_from_sso( superimpose_array, pose, true );
	else {
		if( selection_.size() && superimpose_on_all() ){
			core::pose::datacache::SpecialSegmentsObserver::set_farray_from_sso( superimpose_array, pose, true );
		}
		for( std::list<core::Size>::const_iterator it = selection_.begin(); it!=selection_.end(); ++it ) {
			selection_array[*it-1] = true; // FArray1D is 0 indexed
			superimpose_array[*it-1] = true;
		}
	}

	if( superimpose_ && !superimpose_on_all() ) {
		if ( symmetry_ ) // SJF I haven't changed symmetry selection_array b/c I don't know which tests to use
			rmsd = sym_rmsd_with_super_subset( copy_pose, native, selection_array, core::scoring::is_protein_CA );
		else
			rmsd = rmsd_with_super_subset( copy_pose, native, superimpose_array, core::scoring::is_protein_CA );
	}else if( superimpose_ && superimpose_on_all() ){
		calpha_superimpose_pose( copy_pose, native );
		rmsd = core::scoring::rmsd_no_super_subset( copy_pose, native, selection_array, core::scoring::is_protein_CA );
	} else {
		rmsd = core::scoring::rmsd_no_super_subset( copy_pose, native, selection_array, core::scoring::is_protein_CA );
	}
	return rmsd;
}

bool
RmsdFilter::apply( core::pose::Pose const & pose ) const {

	core::Real const rmsd( compute( pose ));
	TR << "RMSD over selected residues: " << rmsd ;
	if( rmsd <= threshold_ )
	{
		TR<<" passing."<<std::endl;
		return( true );
	}
	else TR<<" failing." << std::endl;
	return( false );
}

void
RmsdFilter::report( std::ostream & out, core::pose::Pose const & pose ) const {
	core::Real const rmsd( compute( pose ));
	out<<"RMSD: " << rmsd<<'\n';
}

core::Real
RmsdFilter::report_sm( core::pose::Pose const & pose ) const {
	core::Real const rmsd( compute( pose ));
	return( (core::Real) rmsd );
}

void
RmsdFilter::parse_my_tag( utility::tag::TagPtr const tag, protocols::moves::DataMap & data_map, protocols::filters::Filters_map const &, protocols::moves::Movers_map const &, core::pose::Pose const & reference_pose )
{
	/// @details
	///if the save pose mover has been instantiated, this filter can calculate the rms
	///against the ref pose
	if( tag->hasOption("reference_name") ){
		reference_pose_ = protocols::rosetta_scripts::saved_reference_pose(tag,data_map );
	}
	else{
		reference_pose_ = new core::pose::Pose( reference_pose );
		if ( basic::options::option[ basic::options::OptionKeys::in::file::native ].user() )
			core::import_pose::pose_from_pdb( *reference_pose_, basic::options::option[ basic::options::OptionKeys::in::file::native ] );
	}

	symmetry_ = tag->getOption<bool>( "symmetry", 0 );
	std::string chains = tag->getOption<std::string>( "chains", "" );
	superimpose_on_all( tag->getOption< bool >( "superimpose_on_all", false ) );
	if( chains != "" ) {
		core::Size chain_start( 0 );
		core::Size chain_end( 0 );

		utility::vector1<core::Size> chain_ends = reference_pose_->conformation().chain_endings();
		chain_ends.push_back( reference_pose_->total_residue() ); // chain_endings() doesn't count last residue as a chain ending (sigh)
		for( std::string::const_iterator chain_it = chains.begin(); chain_it != chains.end(); ++chain_it ) { // for each chain letter
			char const chain = *chain_it;
			TR.Debug << "Chain " << chain << " selected" << std::endl;
			for( utility::vector1<core::Size>::const_iterator it = chain_ends.begin(); it != chain_ends.end(); ++it ) {
				chain_end = *it;
				core::Size const chainid = reference_pose.residue( chain_end ).chain();
				if( reference_pose_->pdb_info()->chain( chain_end ) == chain ) { // if chain letter of chain_end == chain specified
					if( chain_end == reference_pose_->total_residue() ) { // all of this because the last residue doesn't count as a chain ending. why, god, why!?
						//core::Size const chainid = reference_pose_.residue( chain_end ).chain();
						for( core::Size i = 1; i <= reference_pose_->total_residue(); ++i ) {
							if( (core::Size)reference_pose_->residue(i).chain() == chainid ) { // first time we hit this, we're at the start of the chain in question
								chain_start = i;
								break;
							}
						}
					}
					else chain_start = reference_pose_->conformation().chain_begin( chainid );
					for( core::Size i = chain_start; i <= chain_end; ++i ) { // populate selection_ list
						selection_.push_back( i );
					}
				}
			}
		}
	}

	utility::vector0< utility::tag::TagPtr > const rmsd_tags( tag->getTags() );
	for( utility::vector0< utility::tag::TagPtr >::const_iterator it=rmsd_tags.begin(); it!=rmsd_tags.end(); ++it ) {
		utility::tag::TagPtr const rmsd_tag = *it;
		if( rmsd_tag->getName() == "residue" ) {
			core::Size const resnum( core::pose::get_resnum( rmsd_tag, *reference_pose_ ) );
			selection_.push_back( resnum );
		}

		if( rmsd_tag->getName() == "span" ) {
			core::Size const begin( core::pose::get_resnum( rmsd_tag, *reference_pose_, "begin_" ) );
			core::Size const end( core::pose::get_resnum( rmsd_tag, *reference_pose_, "end_" ) );
			runtime_assert( end > begin );
			runtime_assert( begin>=1);
			runtime_assert( end<=reference_pose_->total_residue() );
			for( core::Size i=begin; i<=end; ++i ) selection_.push_back( i );
		}

		if( rmsd_tag->getName() == "span_two" ) {
			specify_both_spans_=true;
		  begin_native_ = rmsd_tag->getOption<core::Size>( "begin_native", 0 );
		  end_native_ = rmsd_tag->getOption<core::Size>( "end_native", 0 );
		  begin_pose_ = rmsd_tag->getOption<core::Size>( "begin_pose", 0 );
		  end_pose_ = rmsd_tag->getOption<core::Size>( "end_pose", 0 );
		  CA_only_ = rmsd_tag->getOption<bool>( "CA_only", true );
			runtime_assert( end_pose_ > begin_pose_ );
			runtime_assert( begin_pose_>=1);
			runtime_assert( end_native_ > begin_native_ );
			runtime_assert( begin_native_>=1);
			runtime_assert( end_native_ - begin_native_ == end_pose_ - begin_pose_ );
		}
	}

	if( selection_.size() > 0 ) selection_.unique(); // make sure our selection list doesn't have redudancies
	TR.Debug << "RMSD selected residues: ";
	if( selection_.size() == 0 ) TR.Debug << "ALL" << std::endl;
	else {
		for( std::list<core::Size>::const_iterator it = selection_.begin(); it != selection_.end(); ++it ) {
			TR.Debug << *it << " ";
		}
	}
	TR.Debug << std::endl;

	superimpose_ = tag->getOption<bool>( "superimpose", 1 );
	threshold_ = tag->getOption<core::Real>( "threshold", 5 );
	if( tag->hasOption("rms_residues_from_pose_cache") ){
		selection_from_segment_cache_ = tag->getOption<bool>( "rms_residues_from_pose_cache", 1 );
		if( selection_.size() != 0 ) std::cerr << "Warning: in rmsd filter tag, both a span selection and the instruction to set the residues from the pose cache is given. Incompatible, defined span will be ignored." << std::endl;
	}

	TR<<"RMSD filter with superimpose=" << superimpose_ << " and threshold="<< threshold_ << " over residues ";
	if( superimpose_on_all() )
		TR<<" superimpose_on_all set to true. Any spans defined will be used only to measure RMSd but the pose will be supreimposed on the reference pose through all residues ";
	if( selection_from_segment_cache_ ) TR << " that are in pose segment observer cache at apply time." << std::endl;
	else if( selection_.size() == 0 ) {
		TR << "ALL" << std::endl;
		for( core::Size i=1; i<=reference_pose.total_residue(); ++i ) selection_.push_back( i );
	}
	else {
		for( std::list<core::Size>::const_iterator it=selection_.begin(); it != selection_.end(); ++it ) TR << *it << " ";
		TR << std::endl;
	}
}

protocols::filters::FilterOP
RmsdFilterCreator::create_filter() const { return new RmsdFilter; }

std::string
RmsdFilterCreator::keyname() const { return "Rmsd"; }


} // filters
} // protein_interface_design
} // devel


