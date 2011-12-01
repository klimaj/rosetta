// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file IO-functionality for enzyme and matching Constraints
/// @brief
/// @author Florian Richter, floric@u.washington.edu, may 2009

// Unit headers
#include <protocols/toolbox/match_enzdes_util/MatchConstraintFileInfo.hh>
#include <protocols/toolbox/match_enzdes_util/EnzCstTemplateRes.hh>

//package headers
#include <protocols/match/downstream/ExternalGeomSampler.hh>
#include <protocols/match/downstream/LigandConformer.hh>
#include <protocols/enzdes/enzdes_util.hh>

//#include <protocols/enzdes/EnzConstraintIO.hh>
//#include <protocols/enzdes/EnzConstraintParameters.hh>

// Project headers
#include <core/conformation/Residue.hh>
//#include <core/chemical/AA.hh> //needed to convert one letter AA codes
#include <core/chemical/ResidueTypeSet.hh> //have to include complete file
//#include <core/pose/Pose.hh>
//#include <core/id/AtomID.hh>
//#include <basic/options/option.hh>
//#include <core/id/SequenceMapping.hh>
#include <core/chemical/Patch.hh>
#include <basic/basic.hh>
//#include <core/io/pdb/pose_io.hh>  //debug only include
//#include <core/pack/dunbrack/RotamerLibrary.hh> //debug only include
//#include <utility/io/izstream.hh>  //debug only include

// numeric headers
#include <numeric/HomogeneousTransform.hh>

// Utility Headers
#include <utility/io/izstream.hh>
#include <utility/string_util.hh>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <basic/Tracer.hh>


// option key includes
// AUTO-REMOVED #include <basic/options/keys/enzdes.OptionKeys.gen.hh>

// AUTO-REMOVED #include <basic/options/keys/enzdes.OptionKeys.gen.hh>// Why was it auto-removed? Can I put it back in?

// AUTO-REMOVED #include <stdlib.h>

#include <utility/vector1.hh>



static basic::Tracer tr("protocols.toolbox.match_enzdes_util.MatchConstraintFileIfo");

namespace protocols {
namespace toolbox{
namespace match_enzdes_util{


/// @brief function to go through a list of restypes and
/// reduce them to chemically identical ones based on the same base_name
/// i.e. this function gets rid of the variant redundancy
void
add_relevant_restypes_to_subset(
	std::set< core::chemical::ResidueTypeCAP > & restype_subset,
	utility::vector1< core::chemical::ResidueTypeCAP > const & restypes,
	core::chemical::ResidueTypeSetCAP restype_set )
{

	using namespace core::chemical;

	std::set< std::string > basename_set;

	for( core::Size i = 1; i <= restypes.size(); ++i ){

		std::string basename = residue_type_base_name( *(restypes[i]) );

		if( basename_set.find( basename ) == basename_set.end() ){

			basename_set.insert( basename );

			restype_subset.insert( &restype_set->name_map( basename ) );

		}
	}
}



GeomSampleInfo::GeomSampleInfo(
	std::string tag ) :
	tag_(tag), function_tag_( "default" ), ideal_val_(0.0), tolerance_(0.0), periodicity_(360.0),
	force_const_(0.0), num_steps_(0), step_size_(0.0)
{}

GeomSampleInfo::GeomSampleInfo(
	core::Real ideal_val,
	core::Real tolerance,
	core::Real force_k,
	core::Real periodicity
) :
tag_(""), function_tag_("default"), ideal_val_(ideal_val), tolerance_(tolerance),
periodicity_(periodicity), force_const_(force_k), num_steps_(0), step_size_(0.0)
{}

bool
GeomSampleInfo::read_data( std::istringstream & line_stream )
{

	utility::vector1< std::string > fields;

	std::string buffer("");

	while( true ){
		line_stream >> buffer;
		if ( line_stream.fail() ) break;
		fields.push_back( buffer );
	}

	if( fields.size() < 4 ){

		tr << "Not enough fields detected for constraint " << tag_ << "." << std::endl;
		return false;
	};


	ideal_val_ = (core::Real ) atof( fields[1].c_str() );
	tolerance_ = (core::Real ) atof( fields[2].c_str() );
	force_const_ = (core::Real ) atof( fields[3].c_str() );
	periodicity_ = (core::Real ) atof( fields[4].c_str() );

	if( (tag_ != "distanceAB:") && ((periodicity_ < 1.0) || (periodicity_ > 360.0)) ){  //safeguard against stupid input
		std::cerr << "Error: illegal periodicity value of " << periodicity_ << " requested for degree of freedom " << tag_ << "." << std::endl;
 		utility_exit_with_message("Illegal periodicity value given. Value must be between 1.0 and 360.0 degrees.");
	}

	if( fields.size() > 4 ){

		//we have to test if fields[5] is an integer
		std::istringstream f5;
		f5.clear();
		f5.str( fields[5] );

		f5 >> num_steps_;
		if( f5.bad() ) num_steps_ = 0;

		//num_steps_ = (core::Size ) atoi( fields[5].c_str() );
		//tr << "for tag " << tag_ << "fields[5] is |" << fields[5] << "| and num_steps is " << num_steps_ << std::endl;
	}

	if( fields[ fields.size() ] == "PERIODIC" ) function_tag_ = "PERIODIC";

	if( num_steps_ != 0 ) step_size_ = tolerance_ / num_steps_;

	tr.Debug  << "data read for GeomSampleInfo with tag " << tag_ << ": ideal_value(" << ideal_val_ << "), tolerance(" << tolerance_ << "), force_k(" << force_const_ << "), periodicity(" << periodicity_ << "), num_steps(" << num_steps_ << "), step size(" << step_size_ << "), function_tag(" << function_tag_ << ")." << std::endl;


	//some safeguards against retared user input
	if( (tolerance_ == 0.0 ) && (num_steps_ != 0 ) ){

		num_steps_ = 0;

			tr << "WARNING: tolerance for constraint " << tag_ << " specified to be 0, yet num_steps specified to be non-0. Ignoring input and setting num_steps to 0." << std::endl;
	}

	return true;

}


utility::vector1< core::Real >
GeomSampleInfo::create_sample_vector() const
{

	core::Size num_ideal_val(1);

	bool distance( tag_ == "distanceAB:");

	//1. figure out the number of ideal values
	if ( (!distance ) && ( periodicity_ != 360.0 ) ) {
		num_ideal_val = static_cast< Size > ( 360.0 / periodicity_ );
	}


	//2. explicity create all the ideal values
	std::list< core::Real > ideal_values;

	//to make sure that there are no duplications
	//through user input of unforeseen weirdness
	std::set< core::Real > seen_values;

	for( int i =  (int) -( num_ideal_val/2) ; i <= (int) ( num_ideal_val/2); ++i ){

		core::Real val = ideal_val_ + ( i * periodicity_ );

		if( !distance) val = basic::unsigned_periodic_range( val, 360.0 );

		if( seen_values.find( val ) == seen_values.end() ){
			ideal_values.push_back( val );
			seen_values.insert( val );
		}
	}

	//sort from lowest to highest because apl sez so
	ideal_values.sort();

	//clear this bc it will be used again
	seen_values.clear();

	//3. build up the diversification samples around each dihedral value
	utility::vector1< core::Real > samples;
	samples.clear();

	tr.Debug << "ideal values for gsi with tag " << tag_ << ", ideal_val " << ideal_val_ << ", and periodicity " << periodicity_ << "are :";

	for( std::list< core::Real >::const_iterator val_it = ideal_values.begin();
			 val_it != ideal_values.end(); ++val_it ){

		tr.Debug << *val_it << ", ";

		for( int i = (int) -num_steps_; i <= (int) num_steps_; ++i ){

			core::Real val =  *val_it + ( i * step_size_ );

			if( !distance) val = basic::unsigned_periodic_range( val, 360.0 );

			if( seen_values.find( val ) == seen_values.end() ){
				samples.push_back( val );
				seen_values.insert( val );
			}
		}
	} //over all ideal values

	tr.Debug << std::endl << " the generated samples are: ";

	for( core::Size i = 1; i <= samples.size(); ++i ){
		tr.Debug << samples[i] << ", ";
	}
	tr.Debug << std::endl;

	return samples;

}



MatchConstraintFileInfo::MatchConstraintFileInfo(
	core::Size index,
	core::chemical::ResidueTypeSetCAP restype_set )
: index_( index ), is_covalent_(false),
	dis_U1D1_( NULL ), ang_U1D2_(NULL), ang_U2D1_(NULL),
	tor_U1D3_(NULL), tor_U3D1_(NULL), tor_U2D2_(NULL),
	restype_set_( restype_set ), native_ (false)
{
	allowed_seqpos_.clear();
	enz_template_res_.clear();
}

MatchConstraintFileInfo::~MatchConstraintFileInfo() {}


utility::vector1< core::chemical::ResidueTypeCAP >
MatchConstraintFileInfo::allowed_restypes( core::Size which_cstres ) const
{

	utility::vector1< core::chemical::ResidueTypeCAP > to_return;

	EnzCstTemplateResCOP template_res = this->enz_cst_template_res( which_cstres );

	for( EnzCstTemplateRes::RestypeToTemplateAtomsMap::const_iterator restype_it = template_res->atom_inds_for_restype_begin(), restype_end = template_res->atom_inds_for_restype_end();
			 restype_it != restype_end; ++restype_it ){

		to_return.push_back( restype_it->first );
	}

	return to_return;
}

utility::vector1< core::Size > const &
MatchConstraintFileInfo::template_atom_inds(
		core::Size which_cstres,
		core::Size which_template_atom,
		core::chemical::ResidueType const & restype ) const
{

	std::map< core::Size, EnzCstTemplateResOP >::const_iterator map_it =  enz_template_res_.find( which_cstres );

	if ( map_it == enz_template_res_.end() ){
		utility_exit_with_message( "template res with code blabla not found in MatchConstraintFileInfo ");
	}

	return map_it->second->atom_inds_for_restype( which_template_atom, &restype );

}

EnzCstTemplateResCOP
MatchConstraintFileInfo::enz_cst_template_res( core::Size template_res ) const
{

	std::map< core::Size, EnzCstTemplateResOP >::const_iterator map_it =  enz_template_res_.find( template_res );

	if ( map_it == enz_template_res_.end() ){
		utility_exit_with_message( "template res with code blabla not found in MatchConstraintFileInfo ");
	}

	return map_it->second;
}


//protocols::match::ExternalGeomSamplerCOP
//MatchConstraintFileInfo::exgs() const {
//	return exgs_;
//}



bool
MatchConstraintFileInfo::read_data( utility::io::izstream & data )
{
	std::istringstream line_stream;

	std::string line, key(""), tag(""),res3;
	core::Size map_id(0);

	//std::cerr << "calling read data for mcfi " << std::endl;

	while( !data.eof() ){

		key = ""; tag = "";
		getline(data,line);

		std::vector< std::string > comment_split = utility::string_split( line, '#' );
		if( comment_split[0] == "" ) continue;
		line_stream.clear();
		line_stream.str( comment_split[0] );
		line_stream >> key;

		//std::cerr << "reading shit, line is " << line << ", key is " << key;
		//Kui Native 110809
		if ( key == "NATIVE"){
			native_ = true;
		}
		else if ( key == "TEMPLATE::" ) {
			line_stream >> tag;
			//tr.Info << "tag is: " << tag << " ";
			if( tag == "ATOM_MAP:") {

				line_stream >> map_id;

				std::map< core::Size, EnzCstTemplateResOP >::iterator map_it = enz_template_res_.find( map_id );

				if( map_it == enz_template_res_.end() ){

					std::pair< core::Size, EnzCstTemplateResOP > to_insert( map_id, new EnzCstTemplateRes( restype_set_ ) );

					enz_template_res_.insert( to_insert );

					map_it = enz_template_res_.find( map_id );
					map_it->second->set_param_index( map_id );
				}

				map_it->second->read_params( line_stream );
			}

			//std::cerr << "  end of file line, tag was " << tag << std::endl;
		}


		else if ( key == "CONSTRAINT::") {
			line_stream >> tag;

			GeomSampleInfoOP gs_info = new GeomSampleInfo( tag );

			if( !gs_info->read_data( line_stream ) ) return false;

			if (tag == "distanceAB:"){

				dis_U1D1_ = gs_info;

				//old convention to declare covalency in file
				if( dis_U1D1_->periodicity() == 1.0 ) is_covalent_ = true;
				else is_covalent_ = false;
			}

			else if (tag == "angle_A:") ang_U1D2_ = gs_info;

			else if (tag == "angle_B:") ang_U2D1_ = gs_info;

			else if (tag == "torsion_A:") tor_U1D3_ = gs_info;

			else if (tag == "torsion_AB:") tor_U2D2_ = gs_info;

			else if (tag == "torsion_B:") tor_U3D1_ = gs_info;

			else{
				std::cerr << "The following line in the cst file with key " << key << " was not recognized and will be ignored: " << std::endl << line << std::endl;
			}

			//std::cerr << "  end of file line, tag was " << tag << std::endl;

		} //if key==CONSTRAINT

		else if ( key == "ALGORITHM_INFO::" ) {

			line_stream >> tag;

			if( !this->process_algorithm_info( tag, data ) ) return false;

		}

		else if ( key == "CST::END") return true;

		else if ( key != "" ){
			std::cerr << "The following line in the cst file with key " << key << " was not recognized and will be ignored: " << std::endl << line << std::endl;
		}

	} //while ( !data.eof )

	//if we get to here, that means the cstfile is corrupted

	return false;

} //read_data


void
MatchConstraintFileInfo::process_data()
{


	for( std::map< core::Size, EnzCstTemplateResOP >::iterator map_it = enz_template_res_.begin();
			 map_it != enz_template_res_.end(); ++map_it ) {

		utility::vector1< std::string > const & res_name3s =
			map_it->second->allowed_res_types();

		std::set< core::chemical::ResidueTypeCAP > restypes_this_res;

		for( core::Size j = 1; j <= res_name3s.size(); ++j ) {

			utility::vector1< core::chemical::ResidueTypeCAP > all_restypes_this_name3 =
				restype_set_->name3_map( res_name3s[j] );

			add_relevant_restypes_to_subset( restypes_this_res, all_restypes_this_name3, restype_set_ );

		}

		for( std::set< core::chemical::ResidueTypeCAP >::iterator set_it = restypes_this_res.begin();
				 set_it != restypes_this_res.end(); ++set_it ){

			map_it->second->determine_atom_inds_for_restype( *set_it );
		}

	} //loop over all Enz_cst_template-res

}


std::list< core::conformation::ResidueCOP >
MatchConstraintFileInfo::inverse_rotamers_against_residue(
	core::Size const target_template,
	core::conformation::ResidueCOP target_conf
) const
{
	runtime_assert( enz_template_res_.size() == 2 );
	core::Size const invrot_template( target_template == 1 ? 2 : 1 );

	//the exgs created based on the cst file info might be wrong if we have to create inverse rotamers
	//of a residue that is the upstream residue in the cstfile
	bool flip_exgs_upstream_downstream_samples( false );
	if( invrot_template == this->upstream_res() ) flip_exgs_upstream_downstream_samples = true;

	std::list< core::conformation::ResidueCOP > to_return;
	core::Size rotcount_buffer(0);

	//1. loop over all allowed restypes of the inverse template
	utility::vector1< core::chemical::ResidueTypeCAP > invrot_restypes(this->allowed_restypes( invrot_template ));

	//if we're dealing with backbone interaction, only build glycine rotamers
	if( this->is_backbone( invrot_template ) ){
		invrot_restypes.clear();
		invrot_restypes.push_back( &(restype_set_->name_map("ALA")) );
		tr << "Only Ala inverse rotamers will be built because it is a backbone interaction." << std::endl;
	}
	for( core::Size ii =1; ii <= invrot_restypes.size(); ++ii ){

		//2 get the relevant atoms through wich the orientation of the two residues
		//is defined in the constraint file
		utility::vector1< utility::vector1< core::Size > > target_template_atom_inds(3), invrot_template_atom_inds(3);
		for( core::Size atct = 1; atct <= 3; ++atct ){
			target_template_atom_inds[atct] = this->template_atom_inds( target_template, atct, target_conf->type() );
			invrot_template_atom_inds[atct] = this->template_atom_inds( invrot_template, atct, *(invrot_restypes[ii]) );
		}

		//3. loop over all possible combinations of atoms in the present residue and the inverse rotamer
		for( core::Size jj = 1; jj <= target_template_atom_inds[1].size(); ++jj ){
			utility::vector1< core::Size > targ_ats(3);
			targ_ats[1] = target_template_atom_inds[1][jj]; targ_ats[2] = target_template_atom_inds[2][jj]; targ_ats[3] = target_template_atom_inds[3][jj];
			for( core::Size kk = 1; kk <= invrot_template_atom_inds[1].size(); ++kk ){

				utility::vector1< core::Size > invrot_ats(3);
				invrot_ats[1] = invrot_template_atom_inds[1][kk]; invrot_ats[2] = invrot_template_atom_inds[2][kk]; invrot_ats[3] = invrot_template_atom_inds[3][kk];

				//4. hand off to other function so code stays readable
				std::list<core::conformation::ResidueCOP > inv_rots_this_combo = this->inverse_rotamers_against_residue( *target_conf, invrot_restypes[ii], targ_ats, invrot_ats, flip_exgs_upstream_downstream_samples );
				to_return.splice( to_return.end(), inv_rots_this_combo  );

			} // kk loop over all possible atoms in the inverse rotamer
		} //jj loop over all possible atoms in the present residue
		tr << to_return.size() - rotcount_buffer << " inverse rotamers were created for restype " << invrot_restypes[ii]->name() << "." << std::endl;
		rotcount_buffer = to_return.size();
	} //ii loop over all allowed restypes of the inverse template
	return to_return;
}

std::list< core::conformation::ResidueCOP >
MatchConstraintFileInfo::inverse_rotamers_against_residue(
	core::conformation::Residue const & target_conf,
	core::chemical::ResidueTypeCAP invrot_restype,
	utility::vector1< core::Size > const & target_ats,
	utility::vector1< core::Size > const & invrot_ats,
	bool const flip_exgs_upstream_downstream_samples
) const
{
	using namespace protocols::match::downstream;

	std::list< core::conformation::ResidueCOP > to_return;

	utility::vector1< core::conformation::ResidueCOP > rotamers( protocols::enzdes::enzutil::bb_independent_rotamers( invrot_restype, true ) ); //This is getting the residue specific inverse rotamers

//	bool no_theozyme_inverse_rotamers( basic::options::option[ basic::options::OptionKeys::enzdes::no_theozyme_inverse_rotamers ]() );
//	if( no_theozyme_inverse_rotamers ) get rid of rotamers ;

	runtime_assert( rotamers.size() > 0 );
	tr << rotamers.size() << " bbindependent rotamers for Residue " << rotamers[1]->type().name() << "." << std::endl;

	core::Size inv_oat1(0), inv_oat2(0), inv_oat3(0);
	rotamers[1]->select_orient_atoms( inv_oat1, inv_oat2, inv_oat3 );

	utility::vector1< LigandConformer > invrot_conformers;
	for( core::Size rotcount(1); rotcount <= rotamers.size(); ++rotcount ){
		invrot_conformers.push_back( LigandConformer() );
		invrot_conformers[ rotcount ].initialize_from_residue( invrot_ats[1], invrot_ats[2], invrot_ats[3], inv_oat1, inv_oat2, inv_oat3, *(rotamers[rotcount]));
	}

	ExternalGeomSampler exgs( *(this->create_exgs()) );
	//runtime_assert( exgs );

	//apparently we have to do some stuff with the sampler
	exgs.set_dis_D1D2(   invrot_conformers[1].atom1_atom2_distance() );
	exgs.set_dis_D2D3(   invrot_conformers[1].atom2_atom3_distance() );
	exgs.set_ang_D1D2D3( invrot_conformers[1].atom1_atom2_atom3_angle() );
	if( flip_exgs_upstream_downstream_samples ) exgs.flip_upstream_downstream_samples();
	exgs.precompute_transforms();

	HTReal ht_start( target_conf.xyz(target_ats[3]), target_conf.xyz(target_ats[2]), target_conf.xyz(target_ats[1]) );

	for ( Size ii = 1; ii <= exgs.n_tor_U3D1_samples(); ++ii ) {
		HTReal ht_ii = ht_start * exgs.transform( HT_tor_U3D1, ii );
		for ( Size jj = 1; jj <= exgs.n_ang_U2D1_samples(); ++jj ) {
			HTReal ht_jj = ht_ii * exgs.transform( HT_ang_U2D1, jj );
			for ( Size kk = 1; kk <= exgs.n_dis_U1D1_samples(); ++kk ) {
				HTReal ht_kk = ht_jj;
				ht_kk.walk_along_z( exgs.dis_U1D1_samples()[ kk ] );
				for ( Size ll = 1; ll <= exgs.n_tor_U2D2_samples(); ++ll ) {
					HTReal ht_ll = ht_kk * exgs.transform( HT_tor_U2D2, ll );
					for ( Size mm = 1; mm <= exgs.n_ang_U1D2_samples(); ++mm ) {
						HTReal ht_mm = ht_ll * exgs.transform( HT_ang_U1D2, mm );
						for ( Size nn = 1; nn <= exgs.n_tor_U1D3_samples(); ++nn ) {
							HTReal ht_nn = ht_mm * exgs.transform( HT_tor_U1D3, nn );

							for( core::Size rotcount(1); rotcount <= rotamers.size(); ++rotcount ){
								core::conformation::ResidueOP rot = new core::conformation::Residue( *(rotamers[rotcount]) );
								for( core::Size atm = 1; atm <= rot->natoms(); ++atm ){
									rot->set_xyz( atm, invrot_conformers[rotcount].coordinate_in_D3_frame( atm, ht_nn ) );
								}
								to_return.push_back( rot );
							} //loop over all rotamers
						} //nn sampler
					} //mm sampler
				}// ll sampler
			} //kk sampler
		} // jj sampler
	} //ii sampler
	return to_return;
}

bool
MatchConstraintFileInfo::process_algorithm_info(
	std::string tag,
	utility::io::izstream & data
){

	// according to apl's request, only allow prespecified tags
	// if you want to read in data for additional tags, you have
	// to specify those here
	if( ( tag != "match") && ( tag != "match_positions" ) && ( tag != "test")  ){
		utility_exit_with_message("Tag "+tag+" not a legal option for ALGORITHM_INFO block.");
	}

	if( algorithm_inputs_.find( tag ) != algorithm_inputs_.end() ){
		tr << "Error: tag " << tag << " was found twice in the cstfile." << std::endl;
		return false;
	}

	utility::vector1< std::string > alg_strings;

	while( !data.eof() ){

		std::string line("");

		getline(data,line);

		//if ( line == "ALGORITHM_INFO::END"){
		if( utility::trimmed_compare( line, "ALGORITHM_INFO::END") ){

			if( alg_strings.size() != 0 ){

				algorithm_inputs_.insert( std::pair< std::string, utility::vector1< std::string > >( tag, alg_strings ) );

			}

			else tr << "WARNING: ALGORITHM_INFO block for " << tag << " seemed to contain no information." << std::endl;

			return true;
		}

		std::vector< std::string > comment_split = utility::string_split( line, '#' );

		if( comment_split[0] != "" ) alg_strings.push_back( comment_split[0] );

	} //while( !data.eof() ){

	tr << "Error, when reading algorithm info block with tag " << tag << ", no ALGORITHM_INFO::END line was found." << std::endl;

	return false;

} //process algorithm_info




protocols::match::downstream::ExternalGeomSamplerCOP
MatchConstraintFileInfo::create_exgs() const
{

	protocols::match::downstream::ExternalGeomSamplerOP exgs = new protocols::match::downstream::ExternalGeomSampler();

	utility::vector1< std::string > tags_undefined_gsi;

	if( dis_U1D1_ ) exgs->set_dis_U1D1_samples( dis_U1D1_->create_sample_vector() );
	else tags_undefined_gsi.push_back( "distanceAB:" );

	if( ang_U1D2_ ) exgs->set_ang_U1D2_samples( ang_U1D2_->create_sample_vector() );
	else tags_undefined_gsi.push_back( "angle_A:" );

	if( ang_U2D1_ ) exgs->set_ang_U2D1_samples( ang_U2D1_->create_sample_vector() );
	else tags_undefined_gsi.push_back( "angle_B:" );

	if( tor_U1D3_ ) exgs->set_tor_U1D3_samples( tor_U1D3_->create_sample_vector() );
	else tags_undefined_gsi.push_back( "torsion_A:" );

	if( tor_U2D2_ ) exgs->set_tor_U2D2_samples( tor_U2D2_->create_sample_vector() );
	else tags_undefined_gsi.push_back( "torsion_AB:" );

	if( tor_U3D1_ ) exgs->set_tor_U3D1_samples( tor_U3D1_->create_sample_vector() );
	else tags_undefined_gsi.push_back( "torsion_B:" );

	if( tags_undefined_gsi.size() != 0 ){

		tr << "WARNING: could not create external geom sampler from file input because not all 6 necessary degrees of freedom are specified.\n The following DOFs are missing specifications: ";

		for( core::Size i = 1; i <= tags_undefined_gsi.size(); ++i ){
			tr << tags_undefined_gsi[i] << ", ";
		}
		tr << "." << std::endl;

		//std::cerr << "setting external geom sampler to null pointer" << std::endl;
		exgs = NULL;
	}

	return exgs;

}



MatchConstraintFileInfoList::MatchConstraintFileInfoList(
	core::chemical::ResidueTypeSetCAP restype_set )
	: active_mcfi_(0), restype_set_( restype_set )
{
	mcfis_.clear();
}


/// @brief temporary implementation for now, only one MCFI supported
bool
MatchConstraintFileInfoList::read_data( utility::io::izstream & data )
{

	//std::cerr << "calling read data for mcfi list " << std::endl;
	active_mcfi_ = 1;

	//mcfis_.clear();

	core::Size new_index = mcfis_.size() + 1;

	MatchConstraintFileInfoOP mcfi = new MatchConstraintFileInfo( new_index, restype_set_ );

	if( mcfi->read_data( data ) ){

		mcfi->process_data();

		mcfis_.push_back( mcfi );

		determine_upstream_restypes();

		return true;
	}

	return false;
}


utility::vector1< MatchConstraintFileInfoCOP > const &
MatchConstraintFileInfoList::mcfis_for_upstream_restype( core::chemical::ResidueTypeCAP restype ) const {

	std::map< core::chemical::ResidueTypeCAP, utility::vector1< MatchConstraintFileInfoCOP > >::const_iterator mcfi_it = mcfis_for_restype_.find( restype );

	if( mcfi_it == mcfis_for_restype_.end() ){
		utility_exit_with_message( " could not find mcfi list for given restype" );
	}

	return mcfi_it->second;

}

std::list< core::conformation::ResidueCOP >
MatchConstraintFileInfoList::inverse_rotamers_against_residue(
	core::Size const target_template,
	core::conformation::ResidueCOP target_conf ) const
{
	std::list< core::conformation::ResidueCOP > to_return;
	for( core::Size i = 1; i <= mcfis_.size(); ++i ){
		if( mcfis_[i]->num_enz_cst_template_res() != 2 ){
			tr << "Can't create inverse rotamers for mcfi " << i << " because it has more or less than 2 template res." << std::endl;
			continue;
		}
		core::Size const invrot_template( target_template == 1 ? 2 : 1 );
		if( std::find( mcfis_[i]->allowed_res_name3s( target_template ).begin(), mcfis_[i]->allowed_res_name3s( target_template ).end(), target_conf->name3() ) == mcfis_[i]->allowed_res_name3s( target_template ).end() ){
			tr << "Can't create inverse rotamers for mcfi " << i << " because it doesn't contain target template for residue " << target_conf->name3() << "." << std::endl;
			continue;
		}
		std::list< core::conformation::ResidueCOP > mcfi_invrots( mcfis_[i]->inverse_rotamers_against_residue( target_template, target_conf ) );
		to_return.splice( to_return.end(), mcfi_invrots  );
	}
	return to_return;
}


void
MatchConstraintFileInfoList::determine_upstream_restypes()
{

	upstream_restypes_.clear();

	mcfis_for_restype_.clear();

	utility::vector1< std::string > gly_vec;
	gly_vec.push_back("GLY");

	std::set< core::chemical::ResidueTypeCAP > restype_temp_set;

	for( core::Size i =1 ; i <= mcfis_.size(); ++i){

		bool is_backbone( mcfis_[i]->is_backbone( mcfis_[i]->upstream_res() ) );

		//in case the mcfi is a backbone interaction, only allow glycine as the restype
		//for this mcfi
		utility::vector1< std::string > const & res_name3s( is_backbone ? gly_vec : mcfis_[i]->allowed_res_name3s( mcfis_[i]->upstream_res() ) );

		std::set< core::chemical::ResidueTypeCAP > restypes_this_mcfi;

		for( core::Size j = 1; j <= res_name3s.size(); ++j ) {

			utility::vector1< core::chemical::ResidueTypeCAP > all_restypes_this_name3 =
				restype_set_->name3_map( res_name3s[j] );

			add_relevant_restypes_to_subset( restypes_this_mcfi, all_restypes_this_name3, restype_set_ );

		}

		//add the restypes_this_mcfi to the total set of upstream residue
		//(if they haven't already been added)
		for( std::set< core::chemical::ResidueTypeCAP >::iterator set_it = restypes_this_mcfi.begin();
				 set_it != restypes_this_mcfi.end(); ++set_it ){

			//build the restype_to_mcfi mapping
			std::map< core::chemical::ResidueTypeCAP, utility::vector1< MatchConstraintFileInfoCOP > >::iterator res_mcfi_it = mcfis_for_restype_.find( *set_it );

			if( res_mcfi_it == mcfis_for_restype_.end() ){

				std::pair<core::chemical::ResidueTypeCAP, utility::vector1< MatchConstraintFileInfoCOP > > to_insert ( *set_it, utility::vector1< MatchConstraintFileInfoCOP >() );

				mcfis_for_restype_.insert( to_insert );

				res_mcfi_it = mcfis_for_restype_.find( *set_it );

			}

			res_mcfi_it->second.push_back( mcfis_[i] );
			//restype_to_mcfi mapping updated

			if( restype_temp_set.find( *set_it ) == restype_temp_set.end() ){
				restype_temp_set.insert( *set_it );
			}

		} //loop over restypes this mcfi

	} //loop over all mcfis


	//finally put all the restypes into the storage vector
	for( std::set< core::chemical::ResidueTypeCAP >::iterator set_it = restype_temp_set.begin();
			 set_it != restype_temp_set.end(); ++set_it ){

		upstream_restypes_.push_back( *set_it );

	}
}




}
}//enzdes
}//protocols
