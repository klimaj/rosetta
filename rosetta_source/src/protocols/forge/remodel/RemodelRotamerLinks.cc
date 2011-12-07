// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

#include <protocols/forge/remodel/RemodelRotamerLinks.hh>
#include <protocols/forge/remodel/RemodelRotamerLinksCreator.hh>
#include <core/pack/rotamer_set/RotamerLinks.hh>

#include <core/chemical/ResidueType.hh>
#include <basic/options/option.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>

#include <basic/Tracer.hh>
#include <utility/tag/Tag.hh>

// option key includes
#include <basic/options/keys/remodel.OptionKeys.gen.hh>

namespace protocols {
namespace forge {
namespace remodel {

using namespace core;
using namespace chemical;
using namespace conformation;
using namespace basic::options;
using namespace pack;
using namespace rotamer_set;
using namespace task;
using namespace operation;

using basic::t_info;
using basic::t_debug;
using basic::t_trace;
static basic::Tracer TR("protocols.forge.remodel.RemodelRotamerLinks",t_info);

TaskOperationOP RemodelRotamerLinksCreator::create_task_operation() const
{
	return new RemodelRotamerLinks;
}

RemodelRotamerLinks::~RemodelRotamerLinks() {}

TaskOperationOP RemodelRotamerLinks::clone() const
{
	return new RemodelRotamerLinks( *this );
}

void
RemodelRotamerLinks::parse_tag( TagPtr /*tag*/ )
{}

void
RemodelRotamerLinks::apply(
	Pose const & pose,
	PackerTask & ptask
) const
{
	Size const nres( pose.total_residue() );
	// setup residue couplings
	RotamerLinksOP links( new RotamerLinks );
	links->resize( nres );

	Size repeat_number = basic::options::option[ OptionKeys::remodel::repeat_structure];
	Size segment_length = nres / repeat_number;

	utility::vector1< utility::vector1< Size > > equiv_pos;

//find all the equivalent positions, first pass iterate over the base
	for (Size res = 1; res<= segment_length ; res++){
		utility::vector1< Size> list;

		for (Size rep = 0; rep < repeat_number; rep++){
			list.push_back(res+(segment_length*rep));
		}
		equiv_pos.push_back(list);
	}


	//second pass, iterate over to populate the entire chain

	for (Size i = 1; i <= nres ; i++){
		Size subcounter = (i%segment_length);
		if (subcounter == 0){
			subcounter = segment_length;
		}

		links->set_equiv(i, equiv_pos[subcounter]);

		//std::cout << "linking " << i << " with " << subcounter << "array with ";
		for (Size k=1; k<= equiv_pos[subcounter].size(); k++){
		//std::cout << " " << equiv_pos[subcounter][k];
		}
	//std::cout << std::endl;
	// check for similarities
	//std::cout << ptask.task_string(pose);

	}

	//std::cout << ptask << std::endl;


	ptask.rotamer_links( links );
}

} // namespace remodel
} // namespace forge
} // namespace protocols

