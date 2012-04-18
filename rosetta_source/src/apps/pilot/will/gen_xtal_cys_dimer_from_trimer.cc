// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// (C) 199x-2009 Rosetta Commons participating institutions and developers.
// For more information, see http://www.rosettacommons.org/.

/// @file /src/apps/pilat/will/genmatch.cc
/// @brief ???

#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
// AUTO-REMOVED #include <basic/options/keys/smhybrid.OptionKeys.gen.hh>
//#include <basic/options/keys/willmatch.OptionKeys.gen.hh>
#include <basic/options/option.hh>
// AUTO-REMOVED #include <basic/options/util.hh>
#include <basic/Tracer.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/util.hh>
#include <core/chemical/VariantType.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/conformation/symmetry/SymDof.hh>
#include <core/conformation/symmetry/SymmData.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/conformation/symmetry/util.hh>
#include <core/import_pose/import_pose.hh>
#include <devel/init.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/io/silent/ScoreFileSilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/kinematics/Stub.hh>
// AUTO-REMOVED #include <core/pack/optimizeH.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/dunbrack/RotamerLibrary.hh>
#include <core/pack/dunbrack/RotamerLibraryScratchSpace.hh>
#include <core/graph/Graph.hh>
#include <core/pack/packer_neighbors.hh>
#include <core/pack/rotamer_set/RotamerSetFactory.hh>
#include <core/pack/rotamer_set/RotamerSet.hh>

// AUTO-REMOVED #include <core/pack/dunbrack/SingleResidueDunbrackLibrary.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/Pose.hh>
#include <core/pose/symmetry/util.hh>
#include <core/pose/util.hh>
#include <core/scoring/dssp/Dssp.hh>
// AUTO-REMOVED #include <core/scoring/Energies.hh>
// AUTO-REMOVED #include <core/scoring/rms_util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
// AUTO-REMOVED #include <core/scoring/ScoringManager.hh>
#include <core/scoring/symmetry/SymmetricScoreFunction.hh>
#include <numeric/conversions.hh>
// AUTO-REMOVED #include <numeric/model_quality/rms.hh>
#include <numeric/random/random.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
// AUTO-REMOVED #include <protocols/simple_moves/symmetry/SetupForSymmetryMover.hh>
#include <protocols/simple_moves/symmetry/SymMinMover.hh>
#include <protocols/simple_moves/symmetry/SymPackRotamersMover.hh>
#include <protocols/scoring/ImplicitFastClashCheck.hh>
#include <protocols/sic_dock/xyzStripeHashPose.hh>
#include <sstream>
// AUTO-REMOVED #include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>
// #include <devel/init.hh>

// #include <core/scoring/constraints/LocalCoordinateConstraint.hh>

#include <utility/vector0.hh>
#include <utility/vector1.hh>

//Auto Headers
#include <platform/types.hh>
#include <core/pose/util.tmpl.hh>
#include <apps/pilot/will/mynamespaces.ihh>
#include <apps/pilot/will/will_util.ihh>

#define CONTACT_D2 36.0

using core::kinematics::Stub;
using protocols::scoring::ImplicitFastClashCheck;
using core::pose::Pose;
using core::conformation::ResidueOP;

static basic::Tracer TR("genxtal");
static core::io::silent::SilentFileData sfd;


inline Real const sqr(Real const r) { return r*r; }
inline Real sigmoidish_neighbor( Real const & sqdist ) {
  if( sqdist > 9.*9. ) {
    return 0.0;
  } else if( sqdist < 6.*6. ) {
    return 1.0;
  } else {
    Real dist = sqrt( sqdist );
    return sqr(1.0  - sqr( (dist - 6.) / (9. - 6.) ) );
  }
}

vector1<Size> get_scanres(Pose const & pose) {
  vector1<Size> scanres;
  //if(basic::options::option[basic::options::OptionKeys::willmatch::residues].user()) {
	//TR << "input scanres!!!!!!" << std::endl;
	//scanres = basic::options::option[basic::options::OptionKeys::willmatch::residues]();
	//} else {
    for(Size i = 1; i <= pose.n_residue(); ++i) {
      if(!pose.residue(i).has("N" )) { continue; }
      if(!pose.residue(i).has("CA")) { continue; }
      if(!pose.residue(i).has("C" )) { continue; }
      if(!pose.residue(i).has("O" )) { continue; }
      if(!pose.residue(i).has("CB")) { continue; }
      if(pose.residue(i).name3()=="PRO") { continue; }
      scanres.push_back(i);
    }
		//}
  return scanres;
}



void dumpsym(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec cen2, string fname) {
  double mxd = 0.0;
  for(Size i = 1; i <= pose.n_residue(); ++i) {
    if( pose.residue(i).nbr_atom_xyz().length() > mxd ) mxd = pose.residue(i).nbr_atom_xyz().length();
  }
  vector1<Vec> seenit;
  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;
  TR << "output" << std::endl;
	vector1<string> ANAME(6);
	ANAME[1] = " N  ";
	ANAME[2] = " CA ";
  ANAME[3] = " C  ";
  ANAME[4] = " O  ";
  ANAME[5] = " CB ";
  ANAME[6] = " SG ";
	string CHAIN = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  ozstream out( fname );
  Size acount=0,rcount=0,ccount=0;
  for(Size i3a = 0; i3a < 3; i3a++) {
    for(Size i2a = 0; i2a < 2; i2a++) {
      for(Size j3a = 0; j3a < 3; j3a++) {
        for(Size j2a = 0; j2a < 2; j2a++) {
          for(Size k3a = 0; k3a < 3; k3a++) {
            for(Size k2a = 0; k2a < 2; k2a++) {
              for(Size l3a = 0; l3a < 3; l3a++) {
                for(Size l2a = 0; l2a < 2; l2a++) {
                  for(Size m3a = 0; m3a < 2; m3a++) {
                    for(Size m2a = 0; m2a < 2; m2a++) {
                      for(Size n3a = 0; n3a < 2; n3a++) {
                        for(Size n2a = 0; n2a < 2; n2a++) {

                          Vec chk( pose.xyz(AtomID(1,1)) );
													chk = R3[i3a]*chk; if(i2a) chk = R2*(chk-cen2)+cen2;
													chk = R3[j3a]*chk; if(j2a) chk = R2*(chk-cen2)+cen2;
													chk = R3[k3a]*chk; if(k2a) chk = R2*(chk-cen2)+cen2;
													chk = R3[l3a]*chk; if(l2a) chk = R2*(chk-cen2)+cen2;
													chk = R3[m3a]*chk; if(m2a) chk = R2*(chk-cen2)+cen2;
													// chk = R3[n3a]*chk; if(n2a) chk = R2*(chk-cen2)+cen2;
                          for(vector1<Vec>::const_iterator i = seenit.begin(); i != seenit.end(); ++i) {
                            if( i->distance_squared(chk) < 1.0 ) goto cont2;
                          }
                          goto done2; cont2: continue; done2:
                          seenit.push_back(chk);

                          Vec zero(0,0,0);
                          zero = R3[i3a]*zero; if(i2a) zero = R2*(zero-cen2)+cen2;
                          zero = R3[j3a]*zero; if(j2a) zero = R2*(zero-cen2)+cen2;
                          zero = R3[k3a]*zero; if(k2a) zero = R2*(zero-cen2)+cen2;
                          zero = R3[l3a]*zero; if(l2a) zero = R2*(zero-cen2)+cen2;
                          zero = R3[m3a]*zero; if(m2a) zero = R2*(zero-cen2)+cen2;
                          zero = R3[n3a]*zero; if(n2a) zero = R2*(zero-cen2)+cen2;
                          if( zero.length() > 6.0 * mxd + 10.0 ) continue;

													char chain = CHAIN[ccount%CHAIN.size()];
                          ccount++;
													// if( (i2a+j2a+k2a+l2a/*+m2a+n2a*/) % 2 == 1 ) chain = 'B';
                          for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
														// if( rcount >= 9999) {
														// 	rcount = 0;
														// 	ccount++;
														// }
                            Size rn = ++rcount;
                            Size natom = 3;
                            if(pose.residue(ir).name3()=="CYS") natom = 6;
                            for(Size ia = 1; ia <= natom; ia++) {
                              Vec tmp(pose.residue(ir).xyz(ia));
                              tmp = R3[i3a]*tmp; if(i2a) tmp = R2*(tmp-cen2)+cen2;
                              tmp = R3[j3a]*tmp; if(j2a) tmp = R2*(tmp-cen2)+cen2;
                              tmp = R3[k3a]*tmp; if(k2a) tmp = R2*(tmp-cen2)+cen2;
                              tmp = R3[l3a]*tmp; if(l2a) tmp = R2*(tmp-cen2)+cen2;
                              tmp = R3[m3a]*tmp; if(m2a) tmp = R2*(tmp-cen2)+cen2;
                              tmp = R3[n3a]*tmp; if(n2a) tmp = R2*(tmp-cen2)+cen2;
                              string X = F(8,3,tmp.x());
                              string Y = F(8,3,tmp.y());
                              string Z = F(8,3,tmp.z());
                              out<<"ATOM  "<<I(5,++acount)<<' '<<ANAME[ia]<<' '<<"ALA"<<' '<<chain<<I(4,rn)<<"    "<<X<<Y<<Z<<F(6,2,1.0)<<F(6,2,0.0)<<'\n';
                            }
                          }
													out << "TER" << std::endl;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  out.close();
}

struct SYMSUB {
  std::string vname;
  Vec X,Y,C;
  int trimer_id;
};

int dumpsymfile_contact(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec cen2, string fname, int & ncontact) {
  double mxd = 0.0;
  for(Size i = 1; i <= pose.n_residue(); ++i) {
    if( pose.residue(i).nbr_atom_xyz().length() > mxd ) mxd = pose.residue(i).nbr_atom_xyz().length();
  }
  vector1<Vec> seenit;
  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;
  Size acount=0,rcount=0,ccount=0;
  vector1<SYMSUB> subs;
  std::map<int,SYMSUB> tsubs;
  ncontact = 0;
  for(Size n2a = 0; n2a < 2; n2a++) {
  for(Size n3a = 0; n3a < 3; n3a++) {
  for(Size m2a = 0; m2a < 2; m2a++) {
  for(Size m3a = 0; m3a < 3; m3a++) {
  for(Size l2a = 0; l2a < 2; l2a++) {
  for(Size l3a = 0; l3a < 3; l3a++) {
  for(Size k2a = 0; k2a < 2; k2a++) {
  for(Size k3a = 0; k3a < 3; k3a++) {
  for(Size j2a = 0; j2a < 2; j2a++) {
  for(Size j3a = 0; j3a < 3; j3a++) {
  for(Size i2a = 0; i2a < 2; i2a++) {
  for(Size i3a = 0; i3a < 3; i3a++) {
    Vec chk( pose.xyz(AtomID(1,1)) );
    chk=R3[i3a]*chk;if(i2a)chk=R2*(chk-cen2)+cen2;chk=R3[j3a]*chk;if(j2a)chk=R2*(chk-cen2)+cen2;chk=R3[k3a]*chk;if(k2a)chk=R2*(chk-cen2)+cen2;
    chk=R3[l3a]*chk;if(l2a)chk=R2*(chk-cen2)+cen2;chk=R3[m3a]*chk;if(m2a)chk=R2*(chk-cen2)+cen2;chk=R3[n3a]*chk;if(n2a)chk=R2*(chk-cen2)+cen2;
    for(vector1<Vec>::const_iterator i=seenit.begin(); i != seenit.end(); ++i) {
      if( i->distance_squared(chk) < 1.0 ) goto cont2;
    }
    goto done2; cont2: continue; done2:
    seenit.push_back(chk);

    Vec zero(0,0,0);
    zero=R3[i3a]*zero;if(i2a)zero=R2*(zero-cen2)+cen2;zero=R3[j3a]*zero;if(j2a)zero=R2*(zero-cen2)+cen2;zero=R3[k3a]*zero;if(k2a)zero=R2*(zero-cen2)+cen2;
    zero=R3[l3a]*zero;if(l2a)zero=R2*(zero-cen2)+cen2;zero=R3[m3a]*zero;if(m2a)zero=R2*(zero-cen2)+cen2;zero=R3[n3a]*zero;if(n2a)zero=R2*(zero-cen2)+cen2;
    if( zero.length() > 2.0 * mxd + 10.0 ) continue;
    bool contact=false;
    for(int i=1; i <= pose.n_residue(); ++i) {
      if(pose.residue(i).aa()==core::chemical::aa_gly||pose.residue(i).aa()==core::chemical::aa_pro) continue;
      Vec pa=R3[0]*pose.xyz(AtomID(5,i));
      pa=R3[i3a]*pa;if(i2a)pa=R2*(pa-cen2)+cen2;pa=R3[j3a]*pa;if(j2a)pa=R2*(pa-cen2)+cen2;pa=R3[k3a]*pa;if(k2a)pa=R2*(pa-cen2)+cen2;
      pa=R3[l3a]*pa;if(l2a)pa=R2*(pa-cen2)+cen2;pa=R3[m3a]*pa;if(m2a)pa=R2*(pa-cen2)+cen2;pa=R3[n3a]*pa;if(n2a)pa=R2*(pa-cen2)+cen2;
      for(int j=1; j <= pose.n_residue(); ++j) {
        if(pose.residue(j).aa()==core::chemical::aa_gly||pose.residue(j).aa()==core::chemical::aa_pro) continue;
        Vec qa = pose.xyz(AtomID(5,j));
        if(pa.distance_squared(qa)<400.0) {
          contact=true;
          if(pa.distance_squared(qa)<64.0) ncontact++;
        }
      }
    }
    if(!contact) continue;

    // cout << n2a<<" "<<n3a<<" "<<m2a<<" "<<m3a<<" "<<l2a<<" "<<l3a<<" "<<k2a<<" "<<k3a<<" "<<j2a<<" "<<j3a<<" "<<i2a<<" "<<i3a << endl;
    SYMSUB s;
    Vec X = Vec(0,0,1);
    X=R3[i3a]*X;if(i2a)X=R2*(X);X=R3[j3a]*X;if(j2a)X=R2*(X);X=R3[k3a]*X;if(k2a)X=R2*(X);
    X=R3[l3a]*X;if(l2a)X=R2*(X);X=R3[m3a]*X;if(m2a)X=R2*(X);X=R3[n3a]*X;if(n2a)X=R2*(X);
    Vec Y = Vec(0,1,0);
    Y=R3[i3a]*Y;if(i2a)Y=R2*(Y);Y=R3[j3a]*Y;if(j2a)Y=R2*(Y);Y=R3[k3a]*Y;if(k2a)Y=R2*(Y);
    Y=R3[l3a]*Y;if(l2a)Y=R2*(Y);Y=R3[m3a]*Y;if(m2a)Y=R2*(Y);Y=R3[n3a]*Y;if(n2a)Y=R2*(Y);
    Vec C = Vec(0,0,0);
    C=R3[i3a]*C;if(i2a)C=R2*(C-cen2)+cen2;C=R3[j3a]*C;if(j2a)C=R2*(C-cen2)+cen2;C=R3[k3a]*C;if(k2a)C=R2*(C-cen2)+cen2;
    C=R3[l3a]*C;if(l2a)C=R2*(C-cen2)+cen2;C=R3[m3a]*C;if(m2a)C=R2*(C-cen2)+cen2;C=R3[n3a]*C;if(n2a)C=R2*(C-cen2)+cen2;
    s.X = X; s.Y = Y; s.C = C;
    s.vname = "S"+str(n2a)+str(n3a)+str(m2a)+str(m3a)+str(l2a)+str(l3a)+str(k2a)+str(k3a)+str(j2a)+str(j3a)+str(i2a)+str(i3a);
    s.trimer_id = 15552*n2a+7776*n3a+2592*m2a+1296*m3a+432*l2a+216*l3a+72*k2a+36*k3a+12*j2a+6*j3a+2*i2a;
    subs.push_back(s);
    if(tsubs.count(s.trimer_id)==0) {
      SYMSUB t;
      t.X = X; t.Y = Y; t.C = C;
      t.vname = "T"+str(n2a)+str(n3a)+str(m2a)+str(m3a)+str(l2a)+str(l3a)+str(k2a)+str(k3a)+str(j2a)+str(j3a)+str(i2a);
      t.trimer_id = 0;
      tsubs[s.trimer_id] = t;
    }

  }}}}}}}}}}}}

  {
    TR << "output" << std::endl;
    ozstream out( fname );
    out << "symmetry_name X23D" << endl;
    out << "anchor_residue 1" << endl;
    out << "E = 1*" << subs[1].vname;
    for(int i = 2; i <= subs.size(); ++i) out << " + 1*(" << subs[1].vname << ":" << subs[i].vname << ")";
    out << endl;
    out << "virtual_coordinates_start" << endl;
    utility::vector1<string> tvnm;
    for(std::map<int,SYMSUB>::const_iterator i = tsubs.begin(); i != tsubs.end(); ++i) {
      Vec X=i->second.X, Y=i->second.Y, C=i->second.C;
      out << "xyz " << "C"+i->second.vname << "  " << X.x()<<","<<X.y()<<","<<X.z() << "  " << Y.x()<<","<<Y.y()<<","<<Y.z() << "  " << C.x()<<","<<C.y()<<","<<C.z() << std::endl;
      out << "xyz " << "P"+i->second.vname << "  " << X.x()<<","<<X.y()<<","<<X.z() << "  " << Y.x()<<","<<Y.y()<<","<<Y.z() << "  " << C.x()<<","<<C.y()<<","<<C.z() << std::endl;
      tvnm.push_back(i->second.vname);
    }
    for(int i = 1; i <= subs.size(); ++i) {
      Vec X=subs[i].X, Y=subs[i].Y, C=subs[i].C;
      out << "xyz " << subs[i].vname << "  " << X.x()<<","<<X.y()<<","<<X.z() << "  " << Y.x()<<","<<Y.y()<<","<<Y.z() << "  " << C.x()<<","<<C.y()<<","<<C.z() << std::endl;
    }
    out << "virtual_coordinates_stop" << endl;
    for(int i = 2; i <= tvnm.size(); ++i) out << "connect_virtual JC" << tvnm[i] << " " << "C"+tvnm[1] << " " << "C"+tvnm[i] << endl;
    for(int i = 1; i <= tvnm.size(); ++i) out << "connect_virtual JP" << tvnm[i] << " " << "C"+tvnm[i] << " " << "P"+tvnm[i] << endl;
    for(int i = 1; i <= subs.size(); ++i) out << "connect_virtual JT" << subs[i].vname << " " << "P"+tsubs[subs[i].trimer_id].vname << " " << subs[i].vname << endl;
    for(int i = 1; i <= subs.size(); ++i) out << "connect_virtual JS" << subs[i].vname << " " << subs[i].vname << " SUBUNIT" << endl;
    out << "set_dof JP" << tvnm[1] << " x(0.0) angle_x(0.0)" << endl;
    out << "set_jump_group JGP";
    for(int i = 1; i <= tvnm.size(); ++i) out << " JP" << tvnm[i];
    out << endl;
    out << "set_jump_group JGS";
    for(int i = 1; i <= subs.size(); ++i) out << " JS" << subs[i].vname;
    out << endl;
    out.close();
  }
  return subs.size();
}

int dumpsymfile_contact3(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec cen2, string fname) {
  double mxd = 0.0;
  for(Size i = 1; i <= pose.n_residue(); ++i) {
    if( pose.residue(i).nbr_atom_xyz().length() > mxd ) mxd = pose.residue(i).nbr_atom_xyz().length();
  }
  vector1<Vec> seenit;
  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;
  Size acount=0,rcount=0,ccount=0;
  vector1<SYMSUB> subs,allsubs;
  for(Size i3a = 0; i3a < 3; i3a++) {
  for(Size i2a = 0; i2a < 2; i2a++) {
  for(Size j3a = 0; j3a < 3; j3a++) {
  for(Size j2a = 0; j2a < 2; j2a++) {
  for(Size k3a = 0; k3a < 3; k3a++) {
  for(Size k2a = 0; k2a < 2; k2a++) {
  for(Size l3a = 0; l3a < 3; l3a++) {
  for(Size l2a = 0; l2a < 2; l2a++) {
  for(Size m3a = 0; m3a < 2; m3a++) {
  for(Size m2a = 0; m2a < 2; m2a++) {
  for(Size n3a = 0; n3a < 2; n3a++) {
  for(Size n2a = 0; n2a < 2; n2a++) {
    Vec chk( pose.xyz(AtomID(1,1)) );
    chk=R3[i3a]*chk; if(i2a) chk=R2*(chk-cen2)+cen2; chk=R3[j3a]*chk; if(j2a) chk=R2*(chk-cen2)+cen2; chk=R3[k3a]*chk; if(k2a) chk=R2*(chk-cen2)+cen2;
    chk=R3[l3a]*chk; if(l2a) chk=R2*(chk-cen2)+cen2; chk=R3[m3a]*chk; if(m2a) chk=R2*(chk-cen2)+cen2; chk=R3[n3a]*chk; if(n2a) chk=R2*(chk-cen2)+cen2;
    for(vector1<Vec>::const_iterator i=seenit.begin(); i != seenit.end(); ++i) {
      if( i->distance_squared(chk) < 1.0 ) goto cont2;
    }
    goto done2; cont2: continue; done2:
    seenit.push_back(chk);

    Vec zero(0,0,0);
    zero=R3[i3a]*zero; if(i2a) zero=R2*(zero-cen2)+cen2; zero=R3[j3a]*zero; if(j2a) zero=R2*(zero-cen2)+cen2; zero=R3[k3a]*zero; if(k2a) zero=R2*(zero-cen2)+cen2;
    zero=R3[l3a]*zero; if(l2a) zero=R2*(zero-cen2)+cen2; zero=R3[m3a]*zero; if(m2a) zero=R2*(zero-cen2)+cen2; zero=R3[n3a]*zero; if(n2a) zero=R2*(zero-cen2)+cen2;
    if( zero.length() > 2.0 * mxd + 10.0 ) continue;
    bool contact=false;
    for(int i=1; i <= pose.n_residue(); ++i) {
      Vec pa=R3[0]*pose.residue(i).nbr_atom_xyz();
      Vec pb=R3[1]*pose.residue(i).nbr_atom_xyz();
      Vec pc=R3[2]*pose.residue(i).nbr_atom_xyz();                
      pa=R3[i3a]*pa; if(i2a) pa=R2*(pa-cen2)+cen2; pa=R3[j3a]*pa; if(j2a) pa=R2*(pa-cen2)+cen2; pa=R3[k3a]*pa; if(k2a) pa=R2*(pa-cen2)+cen2;
      pa=R3[l3a]*pa; if(l2a) pa=R2*(pa-cen2)+cen2; pa=R3[m3a]*pa; if(m2a) pa=R2*(pa-cen2)+cen2; pa=R3[n3a]*pa; if(n2a) pa=R2*(pa-cen2)+cen2;
      pb=R3[i3a]*pb; if(i2a) pb=R2*(pb-cen2)+cen2; pb=R3[j3a]*pb; if(j2a) pb=R2*(pb-cen2)+cen2; pb=R3[k3a]*pb; if(k2a) pb=R2*(pb-cen2)+cen2;
      pb=R3[l3a]*pb; if(l2a) pb=R2*(pb-cen2)+cen2; pb=R3[m3a]*pb; if(m2a) pb=R2*(pb-cen2)+cen2; pb=R3[n3a]*pb; if(n2a) pb=R2*(pb-cen2)+cen2;
      pc=R3[i3a]*pc; if(i2a) pc=R2*(pc-cen2)+cen2; pc=R3[j3a]*pc; if(j2a) pc=R2*(pc-cen2)+cen2; pc=R3[k3a]*pc; if(k2a) pc=R2*(pc-cen2)+cen2;
      pc=R3[l3a]*pc; if(l2a) pc=R2*(pc-cen2)+cen2; pc=R3[m3a]*pc; if(m2a) pc=R2*(pc-cen2)+cen2; pc=R3[n3a]*pc; if(n2a) pc=R2*(pc-cen2)+cen2;
      for(int j=1; j <= pose.n_residue(); ++j) {
        Vec qa=R3[0]*pose.residue(j).nbr_atom_xyz();
        Vec qb=R3[1]*pose.residue(j).nbr_atom_xyz();
        Vec qc=R3[2]*pose.residue(j).nbr_atom_xyz();                
        if(pa.distance_squared(qa)<200.0||pb.distance_squared(qa)<200.0||pc.distance_squared(qa)<200.0||
           pa.distance_squared(qb)<200.0||pb.distance_squared(qb)<200.0||pc.distance_squared(qb)<200.0||
           pa.distance_squared(qc)<200.0||pb.distance_squared(qc)<200.0||pc.distance_squared(qc)<200.0){
          contact=true;
          break;
        }
      }
      if(contact) break;
    }
    if(!contact) continue;

    SYMSUB s;
    Vec X = Vec(0,0,1);
    X=R3[i3a]*X;if(i2a)X=R2*(X);X=R3[j3a]*X;if(j2a)X=R2*(X);X=R3[k3a]*X;if(k2a)X=R2*(X);
    X=R3[l3a]*X;if(l2a)X=R2*(X);X=R3[m3a]*X;if(m2a)X=R2*(X);X=R3[n3a]*X;if(n2a)X=R2*(X);
    Vec Y = Vec(0,1,0);
    Y=R3[i3a]*Y;if(i2a)Y=R2*(Y);Y=R3[j3a]*Y;if(j2a)Y=R2*(Y);Y=R3[k3a]*Y;if(k2a)Y=R2*(Y);
    Y=R3[l3a]*Y;if(l2a)Y=R2*(Y);Y=R3[m3a]*Y;if(m2a)Y=R2*(Y);Y=R3[n3a]*Y;if(n2a)Y=R2*(Y);
    Vec C = Vec(0,0,0);
    C=R3[i3a]*C;if(i2a)C=R2*(C-cen2)+cen2;C=R3[j3a]*C;if(j2a)C=R2*(C-cen2)+cen2;C=R3[k3a]*C;if(k2a)C=R2*(C-cen2)+cen2;
    C=R3[l3a]*C;if(l2a)C=R2*(C-cen2)+cen2;C=R3[m3a]*C;if(m2a)C=R2*(C-cen2)+cen2;C=R3[n3a]*C;if(n2a)C=R2*(C-cen2)+cen2;
    s.X = X;
    s.Y = Y;
    s.C = C;
    s.vname = "S"+str(i3a)+str(i2a)+str(j3a)+str(j2a)+str(k3a)+str(k2a)+str(l3a)+str(l2a)+str(m3a)+str(m2a)+str(n3a)+str(n2a);
    allsubs.push_back(s);

    subs.push_back(s);

  }}}}}}}}}}}}

  {
    TR << "output" << std::endl;
    ozstream out( fname );
    out << "symmetry_name X23D" << endl;
    out << "anchor_residue 1" << endl;
    out << "E = 1*" << subs[1].vname;
    for(int i = 2; i <= subs.size(); ++i) out << " + 1*(" << subs[1].vname << ":" << subs[i].vname << ")";
    out << endl;
    out << "virtual_coordinates_start" << endl;
    for(int i = 1; i <= subs.size(); ++i) {
      Vec X=subs[i].X, Y=subs[i].Y, C=subs[i].C;
      out << "xyz " << subs[i].vname << "  " << X.x()<<","<<X.y()<<","<<X.z() << "  " << Y.x()<<","<<Y.y()<<","<<Y.z() << "  " << C.x()<<","<<C.y()<<","<<C.z() << std::endl;
    }
    out << "virtual_coordinates_stop" << endl;
    for(int i = 2; i <= subs.size(); ++i) out << "connect_virtual J" << subs[i].vname << " " << subs[1].vname << " " << subs[i].vname << endl;
    for(int i = 1; i <= subs.size(); ++i) out << "connect_virtual JS" << subs[i].vname << " " << subs[i].vname << " SUBUNIT" << endl;
    out << "set_jump_group JGS";
    for(int i = 1; i <= subs.size(); ++i) out << " JS" << subs[i].vname;
    out << endl;
    out.close();
  }
  return subs.size();
}

void dumpsymfile_minimal(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec cen2, string fname) {
  vector1<Vec> seenit;
  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;
  Size acount=0,rcount=0,ccount=0;
  vector1<SYMSUB> subs;
  for(Size i3a = 0; i3a < 3; i3a++) {
  for(Size i2a = 0; i2a < 2; i2a++) {
  for(Size j3a = 0; j3a < 3; j3a++) {
    Vec chk( pose.xyz(AtomID(1,1)) );
    chk = R3[i3a]*chk; if(i2a) chk = R2*(chk-cen2)+cen2;
    chk = R3[j3a]*chk;;
    for(vector1<Vec>::const_iterator i = seenit.begin(); i != seenit.end(); ++i) {
      if( i->distance_squared(chk) < 1.0 ) goto cont2;
    }
    goto done2; cont2: continue; done2:
    seenit.push_back(chk);

    SYMSUB s;
    Vec X = Vec(0,0,1);
    X=R3[i3a]*X;if(i2a)X=R2*(X);X=R3[j3a]*X;
    Vec Y = Vec(0,1,0);
    Y=R3[i3a]*Y;if(i2a)Y=R2*(Y);Y=R3[j3a]*Y;
    Vec C = Vec(0,0,0);
    C=R3[i3a]*C;if(i2a)C=R2*(C-cen2)+cen2;C=R3[j3a]*C;
    s.X = X;
    s.Y = Y;
    s.C = C;
    s.vname = "S"+str(i3a)+str(i2a)+str(j3a);
    subs.push_back(s);

  }}}
  TR << "output" << std::endl;
  ozstream out( fname );
  out << "symmetry_name X23D" << endl;
  out << "anchor_residue 1" << endl;
  out << "E = 1*" << subs[1].vname;
  for(int i = 2; i <= subs.size(); ++i) out << " + 1*(" << subs[1].vname << ":" << subs[i].vname << ")";
  out << endl;
  out << "virtual_coordinates_start" << endl;
  for(int i = 1; i <= subs.size(); ++i) {
    Vec X=subs[i].X, Y=subs[i].Y, C=subs[i].C;
    out << "xyz " << subs[i].vname << "  " << X.x()<<","<<X.y()<<","<<X.z() << "  " << Y.x()<<","<<Y.y()<<","<<Y.z() << "  " << C.x()<<","<<C.y()<<","<<C.z() << std::endl;
  }
  out << "virtual_coordinates_stop" << endl;
  for(int i = 2; i <= subs.size(); ++i) out << "connect_virtual J" << subs[i].vname << " " << subs[1].vname << " " << subs[i].vname << endl;
  for(int i = 1; i <= subs.size(); ++i) out << "connect_virtual JS" << subs[i].vname << " " << subs[i].vname << " SUBUNIT" << endl;
  out << "set_jump_group JGS";
  for(int i = 1; i <= subs.size(); ++i) out << " JS" << subs[i].vname;
  out << endl;
  
  out.close();
}


void dumpsym6(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec HG, string fname, Size irsd, Size ch1, Size ch2, Real ANG) {
  vector1<Vec> seenit;
  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;
  TR << "output" << std::endl;
  for(Size i3a = 0; i3a < 3; i3a++) {
    for(Size i2a = 0; i2a < 2; i2a++) {
      for(Size j3a = 0; j3a < 3; j3a++) {
        for(Size j2a = 0; j2a < 2; j2a++) {
          for(Size k3a = 0; k3a < 3; k3a++) {
            for(Size k2a = 0; k2a < 2; k2a++) {
              for(Size l3a = 0; l3a < 3; l3a++) {
                for(Size l2a = 0; l2a < 2; l2a++) {
                  for(Size m3a = 0; m3a < 2; m3a++) {
                    for(Size m2a = 0; m2a < 2; m2a++) {
                      for(Size n3a = 0; n3a < 2; n3a++) {
                        for(Size n2a = 0; n2a < 2; n2a++) {
                          Pose tmp(pose);
                          rot_pose(tmp,R3[i3a]); if(i2a) rot_pose(tmp,R2,HG);
                          rot_pose(tmp,R3[j3a]); if(j2a) rot_pose(tmp,R2,HG);
                          rot_pose(tmp,R3[k3a]); if(k2a) rot_pose(tmp,R2,HG);
                          rot_pose(tmp,R3[l3a]); if(l2a) rot_pose(tmp,R2,HG);
                          rot_pose(tmp,R3[m3a]); if(m2a) rot_pose(tmp,R2,HG);
                          rot_pose(tmp,R3[n3a]); if(n2a) rot_pose(tmp,R2,HG);
                          Vec chk( tmp.xyz(AtomID(1,1)) );
                          for(vector1<Vec>::const_iterator i = seenit.begin(); i != seenit.end(); ++i) {
                            if( i->distance_squared(chk) < 1.0 ) goto cont2;
                          }
                          goto done2; cont2: continue; done2:
                          seenit.push_back(chk);
                          tmp.dump_pdb( utility::file_basename(fname)
                                        +"_"+  F(4,1,ANG)+"_"+ lzs(irsd,3)+"_"+ lzs((Size)ch1,3)+"_"+ lzs((Size)ch2,3)+"_"+
                                        lzs(i3a,1)+"_"+lzs(i2a,1)+"_"+lzs(j3a,1)+"_"+lzs(j2a,1)+"_"+lzs(k3a,1)+"_"+lzs(k2a,1)+"_"+
                                        lzs(l3a,1)+"_"+lzs(l2a,1)+"_"+lzs(m3a,1)+"_"+lzs(m2a,1)+"_"+lzs(n3a,1)+"_"+lzs(n2a,1)+".pdb");
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void dumpsym2(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec HG, string fname, Size irsd, Size ch1, Size ch2, Real ANG) {
  vector1<Vec> seenit;
  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;
  TR << "output" << std::endl;
  for(Size i3a = 0; i3a < 3; i3a++) {
    for(Size i2a = 0; i2a < 2; i2a++) {
      for(Size j3a = 0; j3a < 3; j3a++) {
        for(Size j2a = 0; j2a < 2; j2a++) {
          Pose tmp(pose);
          rot_pose(tmp,R3[i3a]); if(i2a) rot_pose(tmp,R2,HG);
          rot_pose(tmp,R3[j3a]); if(j2a) rot_pose(tmp,R2,HG);
          Vec chk( tmp.xyz(AtomID(1,1)) );
          for(vector1<Vec>::const_iterator i = seenit.begin(); i != seenit.end(); ++i) {
            if( i->distance_squared(chk) < 1.0 ) goto cont2;
          }
          goto done2; cont2: continue; done2:
          seenit.push_back(chk);
          tmp.dump_pdb( utility::file_basename(fname)
                        +"_"+  F(4,1,ANG)+"_"+ lzs(irsd,3)+"_"+ lzs((Size)ch1,3)+"_"+ lzs((Size)ch2,3)+"_"+
                        lzs(i3a,1)+"_"+lzs(i2a,1)+"_"+lzs(j3a,1)+"_"+lzs(j2a,1)+".pdb");
        }
      }
    }
  }
}

bool symclash(Pose const & pose, Mat R2, Mat R3a, Mat R3b, Vec C2, 
  protocols::sic_dock::xyzStripeHashPose const & xh2,
  protocols::sic_dock::xyzStripeHashPose const & xh3
){
  Vec com(0,0,0);
  for(Size ir = 1; ir <= pose.n_residue(); ++ir) com += pose.xyz(AtomID(2,ir));
  com /= pose.n_residue();
  Real mxd = 0;
  for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
    if( pose.xyz(AtomID(5,ir)).distance(com) > mxd ) mxd = pose.xyz(AtomID(5,ir)).distance(com);
  }
  mxd = (2*mxd+4.0)*(2*mxd+4.0);


  Mat R3[3];
  R3[0] = Mat::identity();
  R3[1] = R3a;
  R3[2] = R3b;

  vector1<Pose> toadd;
  vector1<Vec> olap,ori1,ori2,toaddcom;
  { // now check xtal symm
    {
      Vec chk( com );
      olap.push_back(        chk);
      olap.push_back(    R3a*chk);
      olap.push_back(    R3b*chk);
      olap.push_back(R2*(    chk-C2)+C2);
      olap.push_back(R2*(R3a*chk-C2)+C2);
      olap.push_back(R2*(R3b*chk-C2)+C2);
      chk = Vec(0,0,1);
      ori1.push_back(       chk);
      ori1.push_back(   R3a*chk);
      ori1.push_back(   R3b*chk);
      ori1.push_back(R2*    chk);
      ori1.push_back(R2*R3a*chk);
      ori1.push_back(R2*R3b*chk);
      chk = Vec(0,1,0);
      ori2.push_back(       chk);
      ori2.push_back(   R3a*chk);
      ori2.push_back(   R3b*chk);
      ori2.push_back(R2*    chk);
      ori2.push_back(R2*R3a*chk);
      ori2.push_back(R2*R3b*chk);
    }
    for(Size i3a = 0; i3a < 3; i3a++) {
      for(Size i2a = 0; i2a < 2; i2a++) {
        for(Size j3a = 0; j3a < 3; j3a++) {
          for(Size j2a = 0; j2a < 2; j2a++) {
            for(Size k3a = 0; k3a < 3; k3a++) {
              for(Size k2a = 0; k2a < 2; k2a++) {
                for(Size l3a = 0; l3a < 3; l3a++) {
                  for(Size l2a = 0; l2a < 2; l2a++) {
                    for(Size m3a = 0; m3a < 3; m3a++) {
                      for(Size m2a = 0; m2a < 2; m2a++) {
                        for(Size n3a = 0; n3a < 3; n3a++) {
                          for(Size n2a = 0; n2a < 2; n2a++) {
                            Vec chk(com),or1(0,0,1),or2(0,1,0);
                            chk = R3[i3a] * chk; if(i2a) chk = R2*(chk-C2)+C2;
                            chk = R3[j3a] * chk; if(j2a) chk = R2*(chk-C2)+C2;
                            chk = R3[k3a] * chk; if(k2a) chk = R2*(chk-C2)+C2;
                            chk = R3[l3a] * chk; if(l2a) chk = R2*(chk-C2)+C2;
                            chk = R3[m3a] * chk; if(m2a) chk = R2*(chk-C2)+C2;
                            chk = R3[n3a] * chk; if(n2a) chk = R2*(chk-C2)+C2;
                            or1 = R3[i3a] * or1; if(i2a) or1 = R2*or1;
                            or1 = R3[j3a] * or1; if(j2a) or1 = R2*or1;
                            or1 = R3[k3a] * or1; if(k2a) or1 = R2*or1;
                            or1 = R3[l3a] * or1; if(l2a) or1 = R2*or1;
                            or1 = R3[m3a] * or1; if(m2a) or1 = R2*or1;
                            or1 = R3[n3a] * or1; if(n2a) or1 = R2*or1;
                            or2 = R3[i3a] * or2; if(i2a) or2 = R2*or2;
                            or2 = R3[j3a] * or2; if(j2a) or2 = R2*or2;
                            or2 = R3[k3a] * or2; if(k2a) or2 = R2*or2;
                            or2 = R3[l3a] * or2; if(l2a) or2 = R2*or2;
                            or2 = R3[m3a] * or2; if(m2a) or2 = R2*or2;
                            or2 = R3[n3a] * or2; if(n2a) or2 = R2*or2;
                            for(vector1<Vec>::const_iterator i0=olap.begin(),i1=ori1.begin(),i2=ori2.begin(); i0 != olap.end(); ++i0,++i1,++i2) {
                              if( i0->distance_squared(chk) < 1.0 && or1.dot( *i1 ) > 0.95 && or2.dot( *i2 ) > 0.95 ) goto cont6;
                            } goto done6; cont6: continue; done6:
                            olap.push_back(chk);
                            ori1.push_back(or1);
                            ori2.push_back(or2);
                            string X = F(8,3,chk.x());
                            string Y = F(8,3,chk.y());
                            string Z = F(8,3,chk.z());
                            //xout<<"ATOM  "<<I(5,1)<<' '<<" CA "<<' '<<"ALA"<<' '<<"Z"<<I(4,1)<<"    "<<X<<Y<<Z<<F(6,2,1.0)<<F(6,2,0.0)<<'\n';
                            if( chk.distance_squared(com) > mxd ) continue;
                            //TR << chk << std::endl;
                            bool contact = false;
                            for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
                              for(Size ia = 1; ia <= 5; ++ia) {
                                Vec X( pose.xyz(AtomID(ia,ir)) );
                                X = R3[i3a] * X; if(i2a) X = R2*(X-C2)+C2;
                                X = R3[j3a] * X; if(j2a) X = R2*(X-C2)+C2;
                                X = R3[k3a] * X; if(k2a) X = R2*(X-C2)+C2;
                                X = R3[l3a] * X; if(l2a) X = R2*(X-C2)+C2;
                                X = R3[m3a] * X; if(m2a) X = R2*(X-C2)+C2;
                                X = R3[n3a] * X; if(n2a) X = R2*(X-C2)+C2;
                                if( xh2.nbcount(X)>0 ) goto cont7;
                                // if( ia == 5 ) {
                                //   for(vector1<Vec>::const_iterator jcb = cba.begin(); jcb != cba.end(); ++jcb) {
                                //     if( jcb->distance_squared(X) < CONTACT_D2 ) {
                                //       contact = true;
                                //     }
                                //   }
                                // }
                              }
                            }
                            // if(contact) {
                            //   Pose tmp(pose);
                            //   //core::util::switch_to_residue_type_set(tmp,"centroid");
                            //   rot_pose(tmp,R3[i3a]); if(i2a) rot_pose(tmp,R2,C2);
                            //   rot_pose(tmp,R3[j3a]); if(j2a) rot_pose(tmp,R2,C2);
                            //   rot_pose(tmp,R3[k3a]); if(k2a) rot_pose(tmp,R2,C2);
                            //   rot_pose(tmp,R3[l3a]); if(l2a) rot_pose(tmp,R2,C2);
                            //   rot_pose(tmp,R3[m3a]); if(m2a) rot_pose(tmp,R2,C2);
                            //   rot_pose(tmp,R3[n3a]); if(n2a) rot_pose(tmp,R2,C2);
                            //   toadd.push_back(tmp);
                            //   toaddcom.push_back(chk);
                            // }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    } goto done7; cont7: return true; done7:
    ;// if(olap.size() < 25) return true;
  }
  return false;
}

vector1<core::Real>
get_chi1(
  core::pose::Pose & pose,
  core::Size irsd
){
  core::pack::rotamer_set::RotamerSetOP rotset;
  core::scoring::ScoreFunction dummy_sfxn;
  dummy_sfxn( pose );
  core::pack::task::PackerTaskOP dummy_task = core::pack::task::TaskFactory::create_packer_task( pose );
  dummy_task->initialize_from_command_line();
  dummy_task->nonconst_residue_task( irsd ).restrict_to_repacking();
  dummy_task->nonconst_residue_task( irsd ).or_include_current( false ); //need to do this because the residue was built from internal coords and is probably crumpled up
  dummy_task->nonconst_residue_task( irsd ).or_fix_his_tautomer( true ); //since we only want rotamers for the specified restype
  core::graph::GraphOP dummy_png = core::pack::create_packer_graph( pose, dummy_sfxn, dummy_task );
  core::pack::rotamer_set::RotamerSetFactory rsf;
  rotset = rsf.create_rotamer_set( pose.residue( irsd ) );
  rotset->set_resid( irsd );
  rotset->build_rotamers( pose, dummy_sfxn, *dummy_task, dummy_png );
  vector1<core::Real> chi1s;
  for(Size krot = 1; krot <= rotset->num_rotamers(); ++krot) {
    core::Real chi1 = rotset->rotamer(krot)->chi(1);
    bool seenit = false;
    for(Size i = 1; i <= chi1s.size(); ++i) {
      if(fabs(chi1s[i]-chi1) < 0.1) seenit = true;
    }
    if(!seenit) chi1s.push_back(chi1);
  }
  return chi1s;
}

#define ATET 54.735610317245360079 // asin(sr2/sr3)
#define AOCT 35.264389682754668343 // asin(sr1/sr3)

inline double fang(Vec const & v) {
  // > cos(    54.735610317245360079)[1] -0.2398983
  // > cos(180-54.735610317245360079)[1]  0.921327
  // > cos(    35.264389682754668343)[1] -0.7603981
  // > cos(180-35.264389682754668343)[1]  0.9753823
  return min(fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-ATET),min(
             fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-AOCT),min(
             fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-180.0+ATET),
             fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-180.0+AOCT))));
}
inline double wang(Vec const & v) {
  if(fang(v)==fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-ATET)) return ATET;
  else if(fang(v)==fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-AOCT)) return AOCT;
  else if(fang(v)==fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-180.0+ATET)) return 180.0-ATET;
  else if(fang(v)==fabs(angle_degrees(v,Vec(0,0,0),Vec(0,0,1))-180.0+AOCT)) return 180.0-AOCT;
  else utility_exit_with_message("FFFFUUUUU");
}
vector1<core::Real>
get_chi2(
  core::pose::Pose const & pose,
  core::Size irsd,
  int idh,
  vector1<Vec> & axes
){
  Vec CB = pose.xyz(AtomID(pose.residue(irsd).atom_index("CB"),irsd));
  Vec SG = pose.xyz(AtomID(pose.residue(irsd).atom_index("SG"),irsd));
  Vec HG = pose.xyz(AtomID(pose.residue(irsd).atom_index("HG"),irsd));
  Vec cax = SG-CB;
  Vec dax = rotation_matrix_degrees(HG-SG,idh?45.0:-45.0) * projperp(HG-SG,CB-SG).normalized();
  if( fabs(dax.dot(HG-SG)) > 0.0001 ) utility_exit_with_message("bad dsf axs " + str(dax.dot(HG-SG)));

  dax.normalize();
  Mat R = rotation_matrix_degrees(cax,0.1);
  vector1<core::Real> chi2s;
  double oldf2 = fang(dax);
  dax = R * dax;
  double oldf1 = fang(dax);
  dax = R * dax;
  for(int i = 1; i < 3601; ++i) {
    double ang = dax.z();
    double f = fang(dax);
    if( oldf1 <= f && oldf1 <= oldf2 && oldf1 < 1.0) {
      // std::cerr << angle_degrees(Vec(0,0,1),Vec(0,0,0),dax) << " " << i << " " << oldf1 << " " << dax.z() << std::endl;
      Vec tmpaxs = dax.cross(Vec(0,0,1));
      double da = wang(dax)-dihedral_degrees(dax,Vec(0,0,0),tmpaxs,Vec(0,0,1));
      // std::cerr << da << std::endl;
      Vec realdax = rotation_matrix_degrees(tmpaxs,-da) * dax;
      if( 90.0 < angle_degrees(realdax,Vec(0,0,0),Vec(0,0,1)) ) realdax *= -1.0;
      // std::cerr << angle_degrees(realdax,Vec(0,0,0),Vec(0,0,1)) << " " << dax << " " << realdax << " " << std::endl;
      if(da > 1.0) continue;
      chi2s.push_back((core::Real)i/10.0);
      axes.push_back(realdax);
    }
    oldf2 = oldf1;
    oldf1 = f;
    dax = R * dax;
  }
  return chi2s;
}



bool check_dsf_dimer(Pose const & pose, Mat R2f, Vec C2f, Size irsd) {
  Vec CB1 = pose.residue(irsd).xyz("CB");
  Vec SG1 = pose.residue(irsd).xyz("SG");
  Vec CB2 = R2f * (CB1-C2f) + C2f;
  Vec SG2 = R2f * (SG1-C2f) + C2f;
  if( fabs(SG1.distance(SG2) - 2.02) > 0.1 ) return false;
  return true;
}



void dock(Pose & init, string fname) {

  using namespace basic::options::OptionKeys;

  core::chemical::ResidueTypeSetCAP  rs = core::chemical::ChemicalManager::get_instance()->residue_type_set( core::chemical::FA_STANDARD );
  Pose cys;

  make_pose_from_sequence(cys,"C","fa_standard",false);
  remove_lower_terminus_type_from_pose_residue(cys,1);
  remove_upper_terminus_type_from_pose_residue(cys,1);
  cys.set_dof(core::id::DOF_ID(core::id::AtomID(cys.residue(1).atom_index("HG"),1),core::id::D    ),1.01);
  cys.set_dof(core::id::DOF_ID(core::id::AtomID(cys.residue(1).atom_index("HG"),1),core::id::THETA),1.322473);

  core::scoring::dssp::Dssp dssp(init);
  dssp.insert_ss_into_pose(init);

  Vec com(0,0,0);
  for(Size ir = 1; ir <= init.n_residue(); ++ir) {
    // init.replace_residue(ir,cys.residue(1),true);
    // replace_pose_residue_copying_existing_coordinates(init,ir,init.residue(ir).residue_type_set().name_map("ALA"));
    com += init.xyz(AtomID(2,ir));
  }
  com /= init.n_residue();

  protocols::sic_dock::xyzStripeHashPose xh2(2.8,init);
  protocols::sic_dock::xyzStripeHashPose xh3(3.5,init);

  Size nres = init.n_residue();
  // ScoreFunctionOP sf = core::scoring::getScoreFunction();
  ScoreFunctionOP sf = new core::scoring::symmetry::SymmetricScoreFunction(core::scoring::getScoreFunction());

  Pose pose = init;

  Mat R3a = rotation_matrix_degrees(Vec(0,0,1), 120.0);
  Mat R3b = rotation_matrix_degrees(Vec(0,0,1),-120.0);

  vector1<Size> scanres = get_scanres(pose);

  core::pack::dunbrack::SingleResidueRotamerLibraryCAP dunlib = core::pack::dunbrack::RotamerLibrary::get_instance().get_rsd_library( rs->name_map("CYS") );
  core::pack::dunbrack::RotamerLibraryScratchSpace scratch;

  for(vector1<Size>::const_iterator iiter = scanres.begin(); iiter != scanres.end(); ++iiter) {
    Size irsd = *iiter;

    // require some SS within 2 of CYS
    for( int iss = int(irsd)-2; iss <= int(irsd)+2; ++iss) {
      if( iss < 1 || iss > pose.n_residue() ) goto contss;
      if( pose.secstruct(iss) != 'L' ) goto doness;
    } goto doness; contss: continue; doness:
    Mat R3f1 = rotation_matrix_degrees(Vec(0,0,1),120.0);
    Mat R3f2 = rotation_matrix_degrees(Vec(0,0,1),240.0);

    ResidueOP rprev = pose.residue(irsd).clone();
    pose.replace_residue(irsd,cys.residue(1),true);
    Vec CB = pose.xyz(AtomID(pose.residue(irsd).atom_index("CB"),irsd));
    vector1<core::Real> chi1s = get_chi1(pose,irsd);
    for(Size krot = 1; krot <= chi1s.size(); ++krot) {
      pose.set_chi(1,irsd,chi1s[krot]);
      Vec SG = pose.xyz(AtomID(pose.residue(irsd).atom_index("SG"),irsd));
      pose.set_chi(2,irsd,0.0);
      for(int idh = 0; idh < 2; idh++) {
        vector1<Vec> axes;
        vector1<core::Real> chi2s = get_chi2(pose,irsd,idh,axes);
        for(Size ich2 = 1; ich2 <= chi2s.size(); ++ich2){
          Vec a2f = axes[ich2];
          pose.set_chi(2,irsd,chi2s[ich2]);
          Vec HG = pose.xyz(AtomID(pose.residue(irsd).atom_index("HG"),irsd));
          Mat R2f = rotation_matrix_degrees(a2f,180.0);

          for(int i = 1; i <= pose.n_residue(); ++i) {
            for(int j = 1; j <= pose.residue(i).last_backbone_atom(); ++j){
              Vec p1 =      (R2f*(pose.xyz(AtomID(j,i))-HG)+HG);
              Vec p2 = R3f1*(R2f*(pose.xyz(AtomID(j,i))-HG)+HG);
              Vec p3 = R3f2*(R2f*(pose.xyz(AtomID(j,i))-HG)+HG);
              if(xh3.nbcount(p1)>0||xh3.nbcount(p2)>0||xh3.nbcount(p3)>0) goto clash1;
            }
          }
          goto noclash1; clash1: continue; noclash1:

          int ncontact = 0;
          for(int i = 1; i <= pose.n_residue(); ++i) {
            if(pose.residue(i).name3()=="GLY") continue;
            // if(pose.secstruct(i)=='L') continue;
            Vec pa = pose.xyz(AtomID(5,i));
            Vec pb = R3f1 * pa;
            Vec pc = R3f2 * pa;
            for(int j = 1; j <= pose.n_residue(); ++j) {
              if(pose.residue(j).name3()=="GLY") continue;
              // if(pose.secstruct(j)=='L') continue;
              Vec qa = R2f * (       pose.xyz(AtomID(5,j)) - HG) + HG;
              Vec qb = R2f * (R3f1 * pose.xyz(AtomID(5,j)) - HG) + HG;
              Vec qc = R2f * (R3f2 * pose.xyz(AtomID(5,j)) - HG) + HG;
              if( pa.distance_squared(qa) < 64.0 ) ncontact++;
              if( pa.distance_squared(qb) < 64.0 ) ncontact++;
              if( pa.distance_squared(qc) < 64.0 ) ncontact++;
              if( pb.distance_squared(qa) < 64.0 ) ncontact++;
              if( pb.distance_squared(qb) < 64.0 ) ncontact++;
              if( pb.distance_squared(qc) < 64.0 ) ncontact++;
              if( pc.distance_squared(qa) < 64.0 ) ncontact++;
              if( pc.distance_squared(qb) < 64.0 ) ncontact++;
              if( pc.distance_squared(qc) < 64.0 ) ncontact++;
            }
          }
          if(ncontact < 20) continue;

          if(symclash(pose,R2f,R3f1,R3f2,HG,xh2,xh3)) continue;

          {
            string sym = "P432";
            double axang = angle_degrees(a2f,Vec(0,0,0),Vec(0,0,1));
            if( fabs(axang-ATET) > 0.1 && fabs(axang-AOCT) > 0.1 ) utility_exit_with_message("bad axis angle!!!!!!! " +str(axang) );
            if( axang > 44.0 ) sym = "I213";
            // Vec AX = rotation_matrix_degrees(HG-SG,idh?45.0:-45.0) * projperp(HG-SG,CB-SG).normalized();
            // pose.set_xyz(AtomID(pose.residue(irsd).atom_index("H"),irsd),HG+a2f);
            string fn = utility::file_basename(fname).substr(0,4)+"_"+str(irsd)+"_"+sym+"_"+str(krot)+"_"+str(idh)+"_"+str(ich2);
            if(check_dsf_dimer(pose,R2f,HG,irsd)) {
              // Pose tmp(pose);
              // rot_pose(tmp,R2f,HG);
              pose.dump_pdb(fn+"_chainA.pdb");
              // tmp .dump_pdb(fn+"B.pdb");
              // dumpsym(pose,R2f,R3f1,R3f2,HG,fn+".pdb");
              int nsymcontact = 0;
              int nsubs = dumpsymfile_contact(pose,R2f,R3f1,R3f2,HG,fn+".sym",nsymcontact);
              // int nsubs = dumpsymfile_contact3(pose,R2f,R3f1,R3f2,HG,fn+"_contact3.sym" );
              // dumpsym(pose,R2f,R3f1,R3f2,HG,fn+".pdb" );
              // utility_exit_with_message("oiarseht");
              std::cerr << "HIT  " << sym << " " << I(4,irsd) << " " << krot << " " << idh << " " << ich2 << " " << I(4,ncontact) << " " << I(4,nsymcontact) << " " << I(4,nsubs) << " " << fn << std::endl;
              // utility_exit_with_message("arst");
            } else {
              std::cerr << "FAIL " << endl;//<< sym << " " << I(4,irsd) << " " << krot << " " << idh << " " << ich2 << " " << I(4,ncontact) << " " << I(4,    0) << " " << fn << std::endl;
            }
          }
        }

      }
    }
    pose.replace_residue(irsd,*rprev,false);
  }

}

int main (int argc, char *argv[]) {
  devel::init(argc,argv);
  using namespace basic::options::OptionKeys;
  for(Size ifn = 1; ifn <= option[in::file::s]().size(); ++ifn) {
    string fn = option[in::file::s]()[ifn];
    Pose pnat;
    core::import_pose::pose_from_pdb(pnat,fn);
    if( pnat.n_residue() > 300 ) continue;
    for(Size ir = 2; ir <= pnat.n_residue()-1; ++ir) {
      if(!pnat.residue(ir).is_protein()) goto cont1;
      if(pnat.residue(ir).is_lower_terminus()) goto cont1;
      if(pnat.residue(ir).is_upper_terminus()) goto cont1;
      if(pnat.residue(ir).name3()=="CYS") goto cont1;
    } goto done1; cont1: TR << "skipping " << fn << std::endl; continue; done1:
		//continue;
    std::cout << "searching " << fn << std::endl;
    dock(pnat,fn);
  }
}

