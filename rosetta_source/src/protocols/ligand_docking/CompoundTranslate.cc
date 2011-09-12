// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief
/// @author Gordon Lemmon (glemmon@gmail.com)

// Unit Headers
#include <protocols/ligand_docking/CompoundTranslate.hh>
#include <protocols/ligand_docking/CompoundTranslateCreator.hh>
#include <protocols/ligand_docking/Translate.hh>
#include <protocols/ligand_docking/grid_functions.hh>
#include <protocols/geometry/RB_geometry.hh>
#include <protocols/moves/RigidBodyMover.hh>

#include <utility/exit.hh>
#include <basic/Tracer.hh>
#include <core/types.hh>
#include <numeric/random/random_permutation.hh>

//Auto Headers
#include <core/grid/CartGrid.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <numeric/xyzVector.io.hh>
#include <utility/tag/Tag.hh>

using basic::T;
using basic::Error;
using basic::Warning;

namespace protocols {
namespace ligand_docking {

static basic::Tracer translate_tracer("protocols.ligand_docking.CompoundTranslate", basic::t_debug);


std::string
CompoundTranslateCreator::keyname() const
{
	return CompoundTranslateCreator::mover_name();
}

protocols::moves::MoverOP
CompoundTranslateCreator::create_mover() const {
	return new CompoundTranslate;
}

std::string
CompoundTranslateCreator::mover_name()
{
	return "CompoundTranslate";
}



///@brief
CompoundTranslate::CompoundTranslate():
		//utility::pointer::ReferenceCount(),
		Mover("CompoundTranslate")
{}

CompoundTranslate::CompoundTranslate(CompoundTranslate const & that):
		//utility::pointer::ReferenceCount(),
		protocols::moves::Mover( that ),
		translates_(that.translates_),
		randomize_order_(that.randomize_order_),
		allow_overlap_(that.allow_overlap_)
{}

CompoundTranslate::~CompoundTranslate() {}

protocols::moves::MoverOP CompoundTranslate::clone() const {
	return new CompoundTranslate( *this );
}

protocols::moves::MoverOP CompoundTranslate::fresh_instance() const {
	return new CompoundTranslate;
}

std::string CompoundTranslate::get_name() const{
	return "CompoundTranslate";
}

///@brief parse XML (specifically in the context of the parser/scripting scheme)
void
CompoundTranslate::parse_my_tag(
		utility::tag::TagPtr const tag,
		protocols::moves::DataMap & datamap,
		protocols::filters::Filters_map const & filters,
		protocols::moves::Movers_map const & movers,
		core::pose::Pose const & pose
)
{
	if ( tag->getName() != "CompoundTranslate" ){
		utility_exit_with_message("This should be impossible");
	}
	if ( ! tag->hasOption("randomize_order")){
		utility_exit_with_message("CompoundTranslate needs a 'randomize_order' option");
	}
	if ( ! tag->hasOption("allow_overlap")){
		utility_exit_with_message("CompoundTranslate needs an 'allow_overlap' option");
	}
	{// parsing randomize_order tag
		std::string allow_overlap_string= tag->getOption<std::string>("randomize_order");
		if(allow_overlap_string == "true" || allow_overlap_string == "True")
			randomize_order_= true;
		else if(allow_overlap_string == "false" || allow_overlap_string == "False")
			randomize_order_= false;
		else utility_exit_with_message("'randomize_order' option takes arguments 'true' or 'false'");
	}
	{// parsing allow_overlap tag
		std::string allow_overlap_string= tag->getOption<std::string>("allow_overlap");
		if(allow_overlap_string == "true" || allow_overlap_string == "True")
			allow_overlap_= true;
		else if(allow_overlap_string == "false" || allow_overlap_string == "False")
			allow_overlap_= false;
		else utility_exit_with_message("'allow_overlap' option takes arguments 'true' or 'false'");
	}

	utility::vector0< utility::tag::TagPtr >::const_iterator begin=tag->getTags().begin();
	utility::vector0< utility::tag::TagPtr >::const_iterator end=tag->getTags().end();
	for(; begin != end; ++begin){
		utility::tag::TagPtr tag= *begin;
		std::string name= tag->getName();
		if( name != "Translate")
			utility_exit_with_message("CompoundTranslate only takes Translate movers");
		TranslateOP translate = new Translate();
		translate->parse_my_tag(tag, datamap, filters, movers, pose);
		translates_.push_back(translate);
	}
}

void CompoundTranslate::apply(core::pose::Pose & pose) {
	if(randomize_order_)
		numeric::random::random_permutation(translates_, numeric::random::RG);

	std::set<core::Size> chains_to_translate;

	TranslateOPs::iterator begin= translates_.begin();
	TranslateOPs::iterator const end= translates_.end();

	for(; begin != end; ++begin){
		TranslateOP translate= *begin;
		core::Size chain_id= translate->get_chain_id(pose);
		chains_to_translate.insert(chain_id);
	}


	if(allow_overlap_){
		for(begin= translates_.begin(); begin != end; ++ begin){
			TranslateOP translate= *begin;
			translate->add_excluded_chains(chains_to_translate.begin(), chains_to_translate.end());
			translate->apply(pose);
		}
	}
	else{ // remove each chain from the exclusion list so that placed chains are in the grid
		for(begin= translates_.begin(); begin != end; ++ begin){
			TranslateOP translate= *begin;
			translate->add_excluded_chains(chains_to_translate.begin(), chains_to_translate.end());
			translate->apply(pose);
			core::Size chain_id= translate->get_chain_id(pose);
			chains_to_translate.erase(chain_id);
		}
	}
}

} //namespace ligand_docking
} //namespace protocols
