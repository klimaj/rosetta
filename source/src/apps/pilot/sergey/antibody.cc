// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file antibody.cc
///
/// @brief C++ port of Python antibody script
/// @author Sergey Lyskov

#include <protocols/antibody/grafting/util.hh>

#ifdef __ANTIBODY_GRAFTING__

#include <protocols/antibody/grafting/antibody_sequence.hh>
#include <protocols/antibody/grafting/regex_based_cdr_detection.hh>
#include <protocols/antibody/grafting/chothia_numberer.hh>
#include <protocols/antibody/grafting/scs_blast.hh>
#include <protocols/antibody/grafting/scs_multi_template.hh>
#include <protocols/antibody/grafting/exception.hh>
#include <protocols/antibody/grafting/scs_functor.hh>
#include <protocols/antibody/grafting/grafter.hh>

// Grafting util function related includes
#include <core/import_pose/import_pose.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <protocols/simple_moves/MutateResidue.hh>
#include <protocols/simple_moves/SuperimposeMover.hh>

#include <basic/report.hh>

#include <basic/options/option_macros.hh>
#include <basic/Tracer.hh>
#include <basic/options/keys/antibody.OptionKeys.gen.hh>
#include <basic/options/option.hh>

//#include <utility/CSI_Sequence.fwd.hh>

#include <devel/init.hh>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include <string>
#include <regex>

//#include <typeinfo>

using std::string;
using core::Size;

static THREAD_LOCAL basic::Tracer TR("antibody");

// Command-line options relevant only to this application
OPT_KEY( String,       heavy )
OPT_KEY( String,       light )
OPT_KEY( Integer,      n)
OPT_KEY( StringVector, multi_template_regions)

void register_options()
{
	NEW_OPT( heavy, "Sequence of antibody heavy chain", "" );
	NEW_OPT( light, "Sequence of antibody light chain", "" );
	NEW_OPT( n, "Number of templates to generate", 1);
	NEW_OPT( multi_template_regions, "Specify sequence regions for which multi-template is generated. Avalible regions are: h1, h2, h3, l1, l2, l3, frh, frl, orientation", "orientation");
}


int antibody_main()
{
	using namespace utility;
	using namespace protocols::antibody::grafting;
	using protocols::antibody::grafting::uint;

	using std::string;

	string heavy_chain_sequence, light_chain_sequence;

	int n_templates = basic::options::option[ basic::options::OptionKeys::n ];

	if ( basic::options::option[ basic::options::OptionKeys::heavy ].user() ) {
		heavy_chain_sequence = basic::options::option[ basic::options::OptionKeys::heavy ]();
	}

	if ( basic::options::option[ basic::options::OptionKeys::light ].user() ) {
		light_chain_sequence = basic::options::option[ basic::options::OptionKeys::light ]();
	}

	if( !heavy_chain_sequence.size()  and  !light_chain_sequence.size() ) {
		// 2BRR, default values for testing
		heavy_chain_sequence = "AVQLEQSGPELKKPGETVKISCKASGYTFTNYGMNWVKQAPGKGLKWMGWINTYTGEPTYADDFKERFAFSLETSASAAYLQINNLKNEDTATYFCARDYYGSTYPYYAMDYWGQGTTVTVSSAKTTAPSVYPLAPVCGDTTGSSVTLGCLVKGYFPEPVTLTWNSGSLSSGVHTFPAVLQSDLYTLSSSVTVTSSTWPSQSITCNVAHPASSTKVDKKIEPRGG";
		light_chain_sequence = "ENVLTQSPAIMSASPGEKVTMTCRASSSVSSSYLHWYQQKSGASPKLWIYSTSNLASGVPARFSGSGSGTSYSLTISSVEAEDAATYYCQQYSGYPYTFGGGTKLEIKRADAAPTVSIFPPSSEQLTSGGASVVCFLNNFYPKDINVKWKIDGSERQNGVLNSWTDQDSKDSTYSMSSTLTLTKDEYERHNSYTCEATHKTSTSPIVKSFNRNEC";
	}

	{
		TR << TR.Red << "Antibody grafting protocol" << TR.Reset << " Running with input:" << std::endl;
		if( n_templates > 1 ) {
			TR << TR.Red << "SCS_MultiTemplate protocol enabled" << TR.Reset << " multi-template regions: ";
			for(auto & s : basic::options::option[basic::options::OptionKeys::multi_template_regions]() ) TR << s << " ";
			TR << std::endl;
		}

		TR << TR.Green << "Heavy chain sequence: " << TR.Bold << heavy_chain_sequence << TR.Reset << std::endl;
		TR << TR.Blue  << "Light chain sequence: " << TR.Bold << light_chain_sequence << TR.Reset << std::endl;

	}

	string const prefix = basic::options::option[basic::options::OptionKeys::antibody::prefix]();

	basic::ReportOP report = std::make_shared<basic::Report>(prefix+"report");

	AntibodySequence as(heavy_chain_sequence, light_chain_sequence);

	RegEx_based_CDR_Detector(report).detect(as);
	std::cout << std::endl;

	AntibodyFramework heavy_fr = as.heavy_framework();
	AntibodyFramework light_fr = as.light_framework();

	std::cout << "Heavy" << heavy_fr << std::endl;
	std::cout << "Light" << light_fr << std::endl;
	std::cout << std::endl;

	try {
		// AntibodyNumbering an( Chothia_Numberer().number(as, heavy_fr, light_fr) );
		// std::cout << "Heavy Numbering: " << an.heavy << std::endl << std::endl;
		// std::cout << "Light Numbering: " << an.heavy << std::endl << std::endl;

		SCS_BlastPlusOP blast = n_templates == 1 ? std::make_shared<SCS_BlastPlus>(report) : std::make_shared<SCS_BlastPlus>();
		blast->init_from_options();

		blast->add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_sequence_length) );
		blast->add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_alignment_length) );
		// JRJ Filters
		//scs.add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_sequence_identity) );
		//scs.add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_template_resolution) );
		//scs.add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_outlier) );
		//scs.add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_template_bfactor) );
		//scs.add_filter( SCS_FunctorCOP(new SCS_BlastFilter_by_OCD) );

		blast->set_sorter( SCS_FunctorCOP(new SCS_BlastComparator_BitScore_Resolution) );

		SCS_BaseOP scs = n_templates == 1 ? blast : SCS_BaseOP(new SCS_MultiTemplate(blast, basic::options::option[basic::options::OptionKeys::multi_template_regions](), report) );

		SCS_ResultsOP scs_results { scs->select(n_templates, as) };

		report->write();

		if( scs_results->frh.size()  and  scs_results->frl.size()  and  scs_results->orientation.size() and
			scs_results->h1.size() and scs_results->h2.size() and scs_results->h3.size() and
			scs_results->l1.size() and scs_results->l2.size() and scs_results->l3.size()
			) {
			TR << "SCS frh best template: " << TR.Bold << TR.Blue  << scs_results->frh[0]->pdb << TR.Reset << std::endl;
			TR << "SCS frl best template: " << TR.Bold << TR.Green << scs_results->frl[0]->pdb << TR.Reset << std::endl;
			TR << "SCS Orientation best template: " << TR.Bold << TR.Yellow << scs_results->orientation[0]->pdb << TR.Reset << std::endl;

			for(int i=0; i<n_templates; ++i) {
				SCS_ResultSet r;
				try { r = scs_results->get_result_set(i); }
				catch(std::out_of_range e) { break; }

				graft_cdr_loops(as, r, prefix, '-'+std::to_string(i), basic::options::option[basic::options::OptionKeys::antibody::grafting_database]() );
			}
		}
		else {
			TR << TR.Red << "Blast+ results for some of the regions are empty!!! Exiting..." << std::endl;
			return 1;
		}
	}
	catch(Grafting_Base_Exception e) {
		std::cout << e << std::endl;
	}

	return 0;
}


int main(int argc, char * argv [])
{
	try {
		register_options();

		basic::options::option.add_relevant( basic::options::OptionKeys::antibody::prefix );
		basic::options::option.add_relevant( basic::options::OptionKeys::antibody::grafting_database );
		basic::options::option.add_relevant( basic::options::OptionKeys::antibody::blastp );

		devel::init(argc, argv);
		return antibody_main();

	} catch ( utility::excn::EXCN_Base const & e ) {
		std::cout << "caught exception " << e.msg() << std::endl;
		return 1;
	}
}


#else // __ANTIBODY_GRAFTING__

#include <iostream>

int main( int /* argc */, char * /* argv */ [] )
{
	std::cerr << "Antibody protocol require to be build with full C++11 support! Please use GCC-4.9+ or Clang-3.6+ and add extras=cxx11 to your scons build command line.";
	return 1;
}

#endif // __ANTIBODY_GRAFTING__
