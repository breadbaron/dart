#include<iostream>
#include<fstream>
#include<stack>
#include<math.h>
#include<ctime>
#include<time.h>

#include "protpal/profile.h"
#include "protpal/reconstruction.h"
#include "protpal/exactMatch.h"
#include "protpal/transducer.h"
#include "protpal/Q.h"
#include "protpal/utils.h"
#include "protpal/ReadProfileScore.h"
#include "protpal/AlignmentSampler.h"
#include "protpal/MyMap.h"
#include "protpal/json.h"

#include "ecfg/ecfgsexpr.h"
#include "tree/phylogeny.h"
#include "ecfg/ecfgmain.h"
#include "seq/alignment.h"
#include "seq/biosequence.h"
#include "util/dexception.h"
#include "util/rnd.h"
#include "util/sexpr.h"

// Main reconstruction program. 

int main(int argc, char* argv[])
{
  
  Json::Value root;   // will contains the root value after parsing.
  Json::Reader reader;
  bool parsingSuccessful = reader.parse( "this.json", root );
  
  exit(0); 
  try{
    
  time_t start,end;
  time (&start);
  // A few utility variables
  node treeNode; 
  vector<Node> children;
  vector<double> branchLengths;   
  vector<string> node_names;
  double verySmall = 1e-5; //proxy for zero-length branches
  double branch_length; 
  string alignString; 
  // create main reconstruction object
  Reconstruction reconstruction(argc, argv);


  // yeccch - I've mostly moved over to using weight_profiles, but some 
  // hackiness still persists! -OW
  // It seems like this is used only in reading transducers in from a file...def. fixable
  vector<string> alphabetVector;
  vector<sstring> toks = reconstruction.alphabet.tokens(); 
  for (vector<sstring>::iterator a=toks.begin(); a!=toks.end(); a++)
    alphabetVector.push_back(string(a->c_str()));


  // If phylogenetic placement was requested, then do this and exit. 
  if ( reconstruction.reads_to_place_filename != "None" )
    {
      ScoreMap scores; 
      if  (reconstruction.score_tabular_filename == "None" )
	{
	  map<int, string> profile_filenames = reconstruction.check_profile_filenames();
	  // limiter = reconstruction.init_limiter();

	  cerr<<"\nProfiles found for the following nodes:\n";
	  for (map<int, string>::iterator fn=profile_filenames.begin(); fn!=profile_filenames.end(); ++fn)
	    if (fn->second == "None")
	      continue;
	    else
	      cerr<< "\t" << reconstruction.tree.node_name[fn->first] << "\t" << fn->second << endl; 
		  
	  cerr<<"Parsing reads..."; 
	  map<string, string> reads = 
	    parse_fasta(reconstruction.reads_to_place_filename.c_str(), reconstruction.alphabet);
	  cerr<<"Done.\n"; 
	  Read read; 
	  for (int profileNode = reconstruction.tree.nodes() -1; profileNode >= 0; 
	       profileNode--)
	    {
	      if (reconstruction.loggingLevel >=1)
		cerr << "Scoring reads to profile at node: " << reconstruction.tree.node_name[profileNode]<<"..."; 
	      if (profile_filenames[profileNode] == "None")
		continue;
	      AbsorbingTransducer ancestralProfile(profile_filenames[profileNode].c_str(),
						   alphabetVector, reconstruction.tree);
	      // not written/parsed in sexpr...ugh.  Fix later
	      ancestralProfile.name = reconstruction.tree.node_name[profileNode]; 
	      ReadProfileScore profile_scorer(&ancestralProfile, 
					      reconstruction.alphabet, reconstruction.rate_matrix);
		      
	      for ( map<string, string>::iterator readIter = reads.begin(); 
		    readIter != reads.end(); ++readIter )
		{
		  read.identifier = readIter->first; 
		  read.set( readIter->second ); 
		  //if ( ! limiter(profileNode, read.identifier) )
		  //			    continue;
		  if (reconstruction.loggingLevel >= 1)
		    cerr<<"\tScoring read/profile pair: " << read.identifier << " - " << profile_scorer.name <<endl; 
		  // Score read to profile, and store in 'scores' map, no logging
		  profile_scorer.score_and_store(read, scores, false);
		  bfloat score; 
		  if (scores.count(read.identifier))
		    if (scores[read.identifier].count(profile_scorer.name))
		      {
			score = scores[read.identifier][profile_scorer.name];
			if (reconstruction.loggingLevel >= 1)
			  cerr<<"\t\tScore:" << score << endl;
			if ( ! bfloat_is_nonzero(score))
			  {
			    cerr<<"ERROR: score is non-positive!\n"; 
			    profile_scorer.clear_DP_matrix(); 
			    profile_scorer.get_score(read, false, //no viterbi
						     true ); // do log
			    exit(0); 
			  }
		      }
		    else
		      cerr << "\t\tRead had no score for profile "<< profile_scorer.name <<endl; 
		  else
		    cerr << "\t\tRead had no score for any profile: "<< read.identifier << endl; 
			   
		}
	      if (reconstruction.loggingLevel >=1)
		cerr<< "done.\n"; 
	    }
	}
      else
	reconstruction.parse_placement_tabular(scores); 
      cerr <<"\n\n"; 
      // Display stuff that got stored in scores, possibly as JSON or simpler tabular
      if (reconstruction.json_placements_filename != "None")
	{
	  ofstream json_file;
	  json_file.open(reconstruction.json_placements_filename.c_str());
	  reconstruction.write_placement_JSON(json_file, scores); 
	}

      if ( ! reconstruction.no_placements_tabular )
	reconstruction.write_placement_tabular(cout, scores); 

      // 		  time(&readEnd); 
      // 		  cout<< "Minutes used to map ";
      // 		  cout << numReads << " reads to a profile: "; 
      // 		  cout << difftime (readEnd, readStart)/60.0 <<endl; 
      exit(0); 
    }
  // END PHYLO-PLACEMENT 

  // Construct a root "posterior profile" if requested.  This requires us to 
  // re-root the tree at that node, and then reconstruct this new "root"
  if ( reconstruction.profile_to_make != "None" )
    {
      const Phylogeny::Node new_root = 
	reconstruction.tree.find_node( reconstruction.profile_to_make.c_str());
      if (new_root < 0)
	{
	  cerr<<"Error: requested profile for node " << reconstruction.profile_to_make;
	  cerr << " does not appear to be a valid node in the tree. \n"; 
	  exit(1); 
	}
      if ( reconstruction.root_profile_filename == "None" )
	{
	  cerr<<"Warning: no file specified for root profile.  Writing to default location: " ;
	  cerr<< reconstruction.profile_to_make << ".sexpr\n"; 
	  reconstruction.root_profile_filename  = reconstruction.profile_to_make + ".sexpr";
	}
      
      if (reconstruction.tree.is_leaf(new_root) )
	{
	  Phylogeny::Node n = reconstruction.tree.add_named_node("", new_root, 1e-5);
          reconstruction.tree.node_name[new_root] = "tmpRoot";
          reconstruction.tree.node_name[n] = reconstruction.profile_to_make; ;
	}
      // Re-root the tree at the desired node, and then proceed with reconstruction
      if ( new_root != reconstruction.tree.root )
	{
	  string tmpFileName = "tree_tmp.newick"; 
	  ofstream tmpTreeFile(tmpFileName.c_str());
	  reconstruction.tree.write(tmpTreeFile, -1, new_root); 
	  tmpTreeFile.close();
	  ifstream tree_file(tmpFileName.c_str());
	  reconstruction.tree.read(tree_file); //tmpFileName.c_str()); 
	  reconstruction.tree.force_binary(); 
	  reconstruction.set_node_names(); 
	  system("rm -f tree_tmp.newick");
	}
    }

  // These  transducers remain the same throughout the traversal, so we can initialize them
  // once and for all and leave them.  
  SingletTrans R(reconstruction.alphabet, reconstruction.rate_matrix, reconstruction.root_insert_prob);
  SplittingTrans Upsilon;

  // If running a simulation was requested, do this instead of reconstruction
  if (reconstruction.simulate)
	{
	  reconstruction.simulate_alignment(); 
	  exit(0); 
	}
  // If generating a phylocomposer input file was requested, do this instead of reconstruction
  if ( reconstruction.phylocomposer_filename != "None" )
    {
      ofstream phylocomp_out;
      phylocomp_out.open( reconstruction.phylocomposer_filename.c_str() );
      if (reconstruction.loggingLevel >= 1)
	cerr<<"Generating phylocomposer file..."; 
      reconstruction.make_sexpr_file(phylocomp_out);
      if (reconstruction.loggingLevel >= 1)
	cerr<<"done.\n"; 
      exit(0); 
    }
  
  // If using an input alignment was requested, do this and bypass reconstruction
  // This is a bit ambiguous now - an 'input alignment' is used for indel counting and other statistics, whereas a 
  // "guide alignment" is used to constrain DP in aligning sequences.  
  else if (reconstruction.input_alignment)
    {
      int seqLength = -1;
      alignString = "";
      for (treeNode = 0; treeNode < reconstruction.tree.nodes(); treeNode++)
	if (reconstruction.sequences.count(reconstruction.tree.node_name[treeNode]))
	  {
	    alignString += reconstruction.tree.node_name[treeNode] + "   " + reconstruction.sequences[reconstruction.tree.node_name[treeNode]] + "\n";
	    if (seqLength != -1)
	      {
		if ((int) reconstruction.sequences[reconstruction.tree.node_name[treeNode]].size() != seqLength)
		  {
		    cerr<<"Error: input alignment's sequences are not all of the same length.  Do not use -ia with unaligned sequences\n"; 
		    exit(1); 
		  }
	      }
	    else 
	      seqLength = reconstruction.sequences[reconstruction.tree.node_name[treeNode]].size();
	  }
    }
  else
    {
      // Otherwise, let the "progressive transducer-profile-based" ancestral reconstruction begin!  
      //  Initialize the exact-match transducers at leaf nodes - can be thought of as a trivial reconstruction
      if(reconstruction.loggingLevel>=1)
	cerr<<"\n";
      vector<Node> leaves = reconstruction.tree.leaf_vector(); 
      // clunky, sorry
      for (int i=0; i< reconstruction.tree.nodes(); i++)
	node_names.push_back(string(reconstruction.tree.node_name[i]));

      if(reconstruction.loggingLevel>=1)
	cerr<<"Making exact-match transducers for leaf nodes...";

      // Verify that all leaf nodes have sequences in the input file
      reconstruction.verify_leaf_sequences();
      
       for (unsigned int i=0; i<leaves.size(); i++)
	 {
 	  treeNode = leaves[i]; 
	   ExactMatch leaf(
			   reconstruction.sequences[reconstruction.tree.node_name[treeNode]], // sequence
			   leaves[i], //tree index
			   reconstruction.alphabet,  // sequence alphabet
			   reconstruction.codon_model // codon model?
			   );
	   // Then, make an absorbing transducer from this, and place it in the profiles map.  
	   AbsorbingTransducer leafAbsorb(&leaf); 
	   ofstream saved_profile;
	   reconstruction.profiles[treeNode] = leafAbsorb; 	  
	   reconstruction.profiles[treeNode].name = reconstruction.tree.node_name[treeNode]; 	  
	 }
      if(reconstruction.loggingLevel>=1)
	cerr<<"Done\n"; 
      // Postorder traversal over internal nodes.  
      // For each internal node:
      //    1. Instantiate its profile using its two child profiles, delete children
      //    2. Fill DP matrix
      //    3. Sample from DP matrix
      //    4. Instantiate an absorbing profile via the sampled profile

      for_nodes_post (reconstruction.tree, reconstruction.tree.root, -1, bi)
	{
	  bool have_read = false;
	  const Phylogeny::Branch& b = *bi;
	  treeNode = b.second; 
	  if (reconstruction.tree.is_leaf(treeNode)) continue; 
	  if (reconstruction.loggingLevel>=1)
	    cerr<<"\nBuilding sequence profile for: "<<reconstruction.tree.node_name[treeNode]<<endl; 
	  children.clear(); 
	  branchLengths.clear(); 

	  stringstream fileToWrite;
	  if ( reconstruction.saved_subtree_profiles_directory != "None" )
	    {
	       fileToWrite << reconstruction.saved_subtree_profiles_directory;
	       fileToWrite << reconstruction.tree.node_name[treeNode] << ".sexpr";
	       if ( FileExists( fileToWrite.str() ))
		 {
		   have_read  = true; 
		   if (reconstruction.loggingLevel >= 1)
		     cerr<<"\tFound stored profile: " << fileToWrite.str() << endl; 
		   AbsorbingTransducer absorbTrans(fileToWrite.str().c_str(),
						    alphabetVector, reconstruction.tree);
		   reconstruction.profiles[treeNode] = absorbTrans; 
		   continue; 
		 }
	    }

	  for_rooted_children(reconstruction.tree, treeNode, child)
	    {
	      children.push_back(*child); 
	      branch_length = min(reconstruction.max_branch_length, max(verySmall, reconstruction.tree.branch_length(treeNode ,*child)));
	      if (branch_length != reconstruction.tree.branch_length(treeNode ,*child))
		{
		  cerr<<"\tNB: branch length "<< reconstruction.tree.branch_length(treeNode ,*child);
		  cerr<<" rounded to "<< branch_length<< endl; 
		}
	      branchLengths.push_back(branch_length); 
	    }

	  // Instantiate the Q transducer object and its prerequisites.  
	  // The two branch transducers must be re-created at each iteration so that the branch lengths
	  // and the depending parameters are correct.  Thus, Q's transitions must be re-built.  
	  // Finally, marginalize Q's null states (e.g. IMDD)
	  

	  BranchTrans B_l(branchLengths[0], // branch length to this child
			  reconstruction.alphabet, // sequence alphabet
			  reconstruction.rate_matrix, // substitution model
			  reconstruction.ins_rate, // insertion rate
			  reconstruction.del_rate, // deletion rate
			  reconstruction.gap_extend, // gap-extend rate
			  reconstruction.sub_rate // substitution-scaling rate (not often used)
			  );
	  BranchTrans B_r(branchLengths[1], // branch length to this child
			  reconstruction.alphabet, // sequence alphabet
			  reconstruction.rate_matrix, // substitution model
			  reconstruction.ins_rate, // insertion rate
			  reconstruction.del_rate, // deletion rate
			  reconstruction.gap_extend, // gap-extend rate
			  reconstruction.sub_rate // substitution-scaling rate (not often used)
			  );

	  // Boy this is awkward.  Can't seem to declare things within an if-else, so I'll declare it outside, then possibly overwrite it
	  // hmm...
	  if ( reconstruction.mixture_2 )
	    {
	      BranchTrans L_mix_2(branchLengths[0], reconstruction.alphabet, reconstruction.rate_matrix, 
			      reconstruction.ins_rate, reconstruction.del_rate, 
				  reconstruction.gap_extend, 
				  1-reconstruction.mix_prior_2, 
				  reconstruction.gap_extend_2,
				  reconstruction.mix_prior_2);

	      BranchTrans R_mix_2(branchLengths[1], reconstruction.alphabet, reconstruction.rate_matrix,
			      reconstruction.ins_rate, reconstruction.del_rate, 
				  reconstruction.gap_extend, 
				  1-reconstruction.mix_prior_2, 
				  reconstruction.gap_extend_2,
				  reconstruction.mix_prior_2);
	      B_l = L_mix_2; 
	      B_r = R_mix_2; 
	    }
	  else if ( reconstruction.mixture_3 )
	    {
	      BranchTrans L_mix_3(branchLengths[0], reconstruction.alphabet, reconstruction.rate_matrix, 
				  reconstruction.ins_rate, reconstruction.del_rate, 
				  reconstruction.gap_extend, 
				  1-reconstruction.mix_prior_2 - reconstruction.mix_prior_3, 
				  reconstruction.gap_extend_2,
				  reconstruction.mix_prior_2, 
				  reconstruction.gap_extend_3,
				  reconstruction.mix_prior_3);

	      BranchTrans R_mix_3(branchLengths[1], reconstruction.alphabet, reconstruction.rate_matrix,
			      reconstruction.ins_rate, reconstruction.del_rate, 
				  reconstruction.gap_extend, 
				  1-reconstruction.mix_prior_2 - reconstruction.mix_prior_3, 
				  reconstruction.gap_extend_2,
				  reconstruction.mix_prior_2, 
				  reconstruction.gap_extend_3,
				  reconstruction.mix_prior_3);
	      B_l = L_mix_3; 
	      B_r = R_mix_3; 
	    }
	  
	  B_l.name ="Left branch";
	  B_r.name ="Right branch";

	  // A Q-transducer is composed of a singlet transducer (R), two branch trans (B_*), and a trivial
	  // splitting transducer (Upsilon). 
	  QTransducer Q(R, //singlet
			B_l, //left-branch
			B_r, // right-branch
			Upsilon, //splitting 
			reconstruction.alphabet //sequence alphabet
			);
	  // Remove non-emitting states in Q (e.g. IMDD - a deletion on both branches)
	  Q.marginalizeNullStates();

	  // Create the new profile at node "treeNode", using the profiles at the children of treeNode:
	  Profile profile(treeNode, // node on the tree where this profile sits
			  reconstruction.profiles[children[0]], // left absorbing transducer
			  reconstruction.profiles[children[1]], // right absorbing transducer
			  Q // Q transducer object - holds R, left branch, right branch, etc
			  );

	  // To be memory-safe, erase the children
	  // "Los Desaparecidos"
	  reconstruction.profiles.erase(children[0]); 
	  reconstruction.profiles.erase(children[1]); 
	  reconstruction.profiles[treeNode].name = reconstruction.tree.node_name[treeNode]; 

	  if (reconstruction.loggingLevel>=1)
	    {
	      cerr<<"\tLeft profile " << profile.left_profile.name << " has: "<<profile.left_profile.num_delete_states;
	      cerr<<" absorbing states.\n"; 
	      cerr<<"\tRight profile " << profile.right_profile.name << " has: "<<profile.right_profile.num_delete_states;
	      cerr<< "  absorbing states.\n"; 
	    }

	  // Somewhat clunky: allow the profile to see the node-name 
	  // mapping that the reconstruction.tree object
	  // holds, for multiple alignment display.  
	  profile.leaves = leaves; 
	  profile.node_names = node_names; 
	  profile.envelope_distance = reconstruction.envelope_distance; 
	  profile.max_sampled_externals = reconstruction.max_sampled_externals; 

	  // If we got a guide alignment on input, let the profile know about
	  // it so it can constrain DP breadth
	  if (reconstruction.guide_alignment_filename != "None")
	    {
	      profile.use_guide_alignment = true; 
	      profile.envelope = &reconstruction.envelope; 
	    }
	  else
	    profile.use_guide_alignment = false; 

	  // Fill the Z matrix via the forward-like algorithm- the only argument is logging level
	  // (Fairly hairy stuff happening within this function)
	  if(reconstruction.loggingLevel>=1)
	    cerr<<"\tFilling forward dynamic programming matrix..."; 

	  if (treeNode != reconstruction.tree.root || reconstruction.root_profile_filename == "None")
	    profile.fill_DP(reconstruction.loggingLevel, // log messages
			    false); // do not store incoming/outgoing information.  
	  else if (reconstruction.root_profile_filename != "None")
	    {
	      if(reconstruction.loggingLevel>=1)
		cerr<<" (logging state connectivity) "; 
	      profile.fill_DP(reconstruction.loggingLevel, // log messages
			      true); // do store incoming/outgoing information - for state postprobs
	      
	      if(reconstruction.loggingLevel>=1)
		cerr<<"\n\tFilling backward DP matrix... "; 	      
	      profile.fill_backward_DP(reconstruction.loggingLevel); 
	    }

	  // Done filling DP matrix - log some useful info about it
	  if(reconstruction.loggingLevel>=1)
	    {
	      cerr<<"done.\n\t\tSubalignment likelihood: "<<-log(profile.forward_prob)/log(2)<<" bits\n"; 
	      cerr<<"\tProfile's DP matrix had " << profile.DP_size() << " cells ("; 
	      cerr<< profile.num_zero_states << " zero-likelihood cells encountered)\n"; 
	      if (profile.num_discarded_states > 0)
		cerr<<"\tEnvelope allowed for discarding " << profile.num_discarded_states << " candidate states during DP recursion\n"; 
	    }


	  // For non-root nodes, we now sample a traceback through the Z matrix.  
	  // This is relatively quick compared to DP
	  // This step also stores the set of states, and the associated transition
	  // information (weights, connectivity)
	  if (treeNode != reconstruction.tree.root) 
	    {
	      if(reconstruction.loggingLevel>=1)
		cerr<<"\tSampling "<<reconstruction.num_sampled_paths<<" alignments from DP matrix..."; 
	      profile.sample_DP(
				reconstruction.num_sampled_paths, // number of paths to get
				reconstruction.loggingLevel, // debugging log messages ?
				reconstruction.show_alignments, // show sampled alignments ?
				reconstruction.leaves_only, // only show leaves
				reconstruction.viterbi // sample viterbi path (on the last time through - only really applies to null states)
				); 
	      // Now that we're done sampling, we can discard the DP matrix.  
	      profile.clear_DP();
	      
	      // Not sure if this code will be ressurected:
	      //	      if (reconstruction.estimate_params)
	      //		reconstruction.pre_summed_profiles[treeNode] = profile; 

	      if(reconstruction.loggingLevel>=1)
		{
		  cerr<<"done.  \n";
		  cerr<<"\tResulting DAG has "<< profile.num_sampled_externals;
		  cerr<< " absorbing states." << endl; 
		}
		  
	      // Transform the (null-in, null-out) transducer into an absorbing transducer:
	      // Remove R-states, modify transition probabilities, sum over null states, 
	      // index remaining delete states.  This is a *mildly* hairy operation...

	      if(reconstruction.loggingLevel>=1)
		cerr<<"\tTransforming sampled profile DAG into an absorbing transducer...";
	      AbsorbingTransducer absorbTrans(&profile);

	      // Make sure transitions/emissions *seem* kosher...
	      absorbTrans.test_transitions(); 

	      // Plunk it down onto the tree, for use in higher-up nodes' profiles. 
	      reconstruction.profiles[treeNode] = absorbTrans; 
	      if(reconstruction.loggingLevel>=1)
		cerr<<"done.\n";
	      
	      // Dot code is a good way to investigate a strangely-behaving transducer
	      // (e.g. if the above transitions test fails)
	      //absorbTrans.show_DOT(cout, reconstruction.tree.node_name[treeNode]+"_absorbing"); 

	      
	      // If requested, save this profile in the specified directory so we can possibly
	      // recycle it in later invocations.  This must be done carefully do avoid inter-dataset
	      // contamination!  
	      if ( reconstruction.saved_subtree_profiles_directory != "None" and not have_read)
		{
		  ofstream saved_profile;
		  state_path dummy_path; 
		  saved_profile.open(fileToWrite.str().c_str()); // is there another way?
		  absorbTrans.write_profile(saved_profile, dummy_path); 
		  if ( reconstruction.loggingLevel >= 1)
		    cerr<<"\tWrote profile to file: " << fileToWrite.str() <<endl; 
		  saved_profile.close();
		}

	      
// 	      CHECK EQUALITY IN READ/WRITTEN TRANSDUCER
// 		absorbTrans2.test_equality(absorbTrans,true, true); 
// 	      DONE READ/WRITE TESTING


	      // TESTING / BENCHMARKING READ-PROFILE SCORING (E.G PPLACER-LIKE FUNCTIONALITY)
// 	      // Writing to hmmoc file - not yet fully functional, but provides a 
// 	      // significant speedup (~10X)
// 	      //testScore.HMMoC_adapter("testHMMoC.xml"); 
// 	      //exit(0); 

// Add back in later:
/*
              time_t readStart,readEnd;
	      time (&readStart);
	      unsigned int numReads = 1;
	      cout<<"\n"; 
*/
	    }
	  else
	    {
	      if (reconstruction.root_profile_filename != "None")
		{
		  if(reconstruction.loggingLevel>=1)
		    cerr<<"\tSampling "<<reconstruction.num_root_alignments<<" alignments from root alignment..."; 
		  profile.sample_DP(
				    reconstruction.num_root_alignments, // number of paths
				    reconstruction.loggingLevel, // debugging log messages ?
				    reconstruction.show_alignments, // show sampled alignments ?
				    reconstruction.leaves_only, // only show leaves
				    reconstruction.viterbi // sample viterbi path (on the last time through - only really applies to null states)
				    ); 
		  if(reconstruction.loggingLevel>=1)
		    cerr<<"Done.\n";
		  state_path viterbi_path;
		  if (reconstruction.root_viterbi_path)
		    viterbi_path = profile.sample_DP(
						       1, // sample only one path
						       0, // debugging log messages
						       false, // don't show alignments
						       false, // leaves only
						       true // sample the viterbi path
						       );
		  if (reconstruction.loggingLevel >=1)
		    cerr<<"\tResulting DAG has "<< profile.num_sampled_externals<< " absorbing states." << endl; 
		  AbsorbingTransducer absorbTrans(&profile);
		  // ********* Write to file ********* 
		  ofstream saved_profile;
		  saved_profile.open(reconstruction.root_profile_filename.c_str() );
		  absorbTrans.write_profile(saved_profile, viterbi_path); 
		  if (reconstruction.loggingLevel >=1)
		    cerr<<"Wrote root profile to file: " << reconstruction.root_profile_filename << endl; 
		  saved_profile.close();
		}
	      
		  
	      cout<<"#=GF alignment_likelihood "<<-log(profile.forward_prob)/log(2) << endl; 
	      ofstream db_file;
	      state_path path = profile.sample_DP(
						  1, // sample only one path
						  0, // debugging log messages
						  false, // don't show alignments
						  false, // leaves only
						  reconstruction.viterbi // sample the viterbi path
						  );
	      alignString = profile.show_alignment( path, reconstruction.leaves_only); 
	      if (reconstruction.num_root_alignments > 1 && reconstruction.indel_filename != "None")
		{
		  if(reconstruction.loggingLevel>=1)
		    cerr<<"\nSampling " << reconstruction.num_root_alignments << " alignments at root level..."; 
		  double tot_ins=0, tot_ins_ext=0, tot_del=0, tot_del_ext=0; 

		  if (reconstruction.db_filename != "None")
		    {		  

		      db_file.open (reconstruction.db_filename.c_str());
		      reconstruction.tree.write_Stockholm(db_file);
		      //alignString = treeStream.str()
		    }
		  for (int samples = 1; samples<= reconstruction.num_root_alignments; samples++)
		    {
		      state_path path = profile.sample_DP(
							  1, // sample only one path
							  0, // debugging log messages
							  false, // don't show alignments
							  false, // leaves only
							  false // don't sample the viterbi path!
							  );
		      if (reconstruction.db_filename != "None")
			db_file << profile.show_alignment(path, reconstruction.leaves_only);
			  
		      MyMap<string, string> alignment = profile.alignment_map(path, false); 
		      IndelCounter indels(alignment, &reconstruction.tree); 
		      indels.gather_indel_info(false); 

		      tot_ins += indels.avg_insert_rate; 
		      if (indels.avg_insert_rate > 0.0)
			tot_ins_ext += indels.avg_insert_ext; 
		      tot_del += indels.avg_delete_rate; 
		      if (indels.avg_delete_rate > 0.0)
			tot_del_ext += indels.avg_delete_ext; 
		      
		      // do a simple average over alignments' inferred param estimates
		      if (samples == reconstruction.num_root_alignments)
			{
			  indels.avg_insert_rate = tot_ins / double(reconstruction.num_root_alignments); 
			  indels.avg_insert_ext = tot_ins_ext / double(reconstruction.num_root_alignments); 
			  indels.avg_delete_rate = tot_del / double(reconstruction.num_root_alignments); 
			  indels.avg_delete_ext = tot_del_ext / double(reconstruction.num_root_alignments); 

			  // write to file
			  ofstream indel_file;
			  indel_file.open (reconstruction.indel_filename.c_str());
			  indel_file << "### Indel information averaged over " << reconstruction.num_root_alignments << " alignments ###"; 
			  indels.display_indel_info(indel_file, false);
			  indel_file.close();

			}
		    }
		  if (reconstruction.db_filename != "None")
		    db_file.close();
		}
	    }
	}
    }
  if(reconstruction.loggingLevel>=1)
    cerr<<"done.\n";

  
  if (reconstruction.loggingLevel >= 1)
    cerr<<"\nFinished with alignment construction, now post-processing for ancestral characters and indels\n"; 
  // There's now an "alignment" created, either from input or via protpal
  // Convert it to a stockholm object - so we can do character recon w/ Xrate
  // There is no way that this is the easiest way to do this, but oh well:
  stringstream treeStream;
  reconstruction.tree.write_Stockholm(treeStream);
  alignString = treeStream.str() + alignString;

  istringstream stockStream(alignString);
  Sequence_database db; 
  Stockholm stk(1,1);
  if (reconstruction.input_alignment)
    stk.read_Stockholm(stockStream,db, 0,"_"); 
  else
    stk.read_Stockholm(stockStream,db);
  
  if (reconstruction.leaves_only)
    stk.write_Stockholm(cout);
  else // display all characters
    {
      ECFG_main ecfg; 
      ecfg.ancrec_CYK_MAP = true; 
      if (reconstruction.ancrec_postprob)
	{
	  ecfg.ancrec_postprob=true;
	  ecfg.min_ancrec_postprob = reconstruction.min_ancrec_postprob; 
	}

      SExpr_file gap_grammar_sexpr_file (reconstruction.gap_grammar_filename.c_str()); 
      SExpr_file grammar_sexpr_file (reconstruction.grammar_filename.c_str()); 

      if (reconstruction.input_alignment)
	{
	  grammar_sexpr_file = gap_grammar_sexpr_file; 
	  ecfg.gap_chars = sstring("_"); 
	}
      
      SExpr& grammar_ecfg_sexpr = grammar_sexpr_file.sexpr;

      // Get the alignment and grammar into the ecfg and get it ready for operations 
      ecfg.read_alignments(stk); 
      ecfg.read_grammars(&grammar_ecfg_sexpr); 
      ecfg.convert_sequences(); 

      // Train the Xrate grammar/chain, if requested
      if (reconstruction.train_grammar)
	{
	  if (reconstruction.loggingLevel >= 1)
	    {
	      cerr<<"\nTraining grammar used for character reconstruction, using starting point: \n\t ";
	      if (reconstruction.input_alignment)
		cerr << reconstruction.gap_grammar_filename << endl;
	      else
		cerr << reconstruction.grammar_filename << endl;
	    }
	  ecfg.train = "/dev/null";
	  ecfg.train_grammars();
	  ecfg.delete_trainers(); 
	}

      // 'Annotate' the alignment, using the current grammar
      if (reconstruction.loggingLevel >=1) 
	cerr<< "\tReconstructing ancestral characters conditional on ML indel history..."; 
      ecfg.annotate_alignments(); 

      Stockholm annotated = *(ecfg.stock_db.align.begin()); 

      if (reconstruction.loggingLevel >=1) 
	{
	  cerr<<"Done.\n";
	  cerr<<"\tDisplaying full ancestral alignment\n\n"; 
	}

      time(&end); 
      cout<< "#=GF TIME_MINUTES " << difftime (end, start)/60.0 <<endl; 
      if (reconstruction.xrate_output || reconstruction.ancrec_postprob)
	annotated.write_Stockholm(cout);
      else // display in bare-bones/ non-Xrate format
	{
	  Phonebook::iterator seq; 
	  int nameSize, maxNameLength = 0; 
	  sstring sequence; 
	  reconstruction.tree.write_Stockholm(cout);
	  for (seq = annotated.row_index.begin(); seq!=annotated.row_index.end(); seq++)
	    {
	      nameSize = seq->first.size();
	      maxNameLength = max( maxNameLength, nameSize );
	    }
	  for (seq = annotated.row_index.begin(); seq!=annotated.row_index.end(); seq++)
	    {
	      if ( reconstruction.tree.is_leaf(index(seq->first, reconstruction.tree.node_name) ) )
		sequence =  annotated.get_row_as_string(seq->second); 
	      else 
		sequence = annotated.gr_annot[seq->first]["ancrec_CYK_MAP"];
	      if (reconstruction.fasta_output)
		cout<< ">" << seq->first << "\n" << sequence << endl; 
	      else
		cout<< seq->first << rep(maxNameLength-seq->first.size()+4," ") << sequence << endl; 
	    }
	}
		  
      // if requested, show what was inserted/deleted on the tree  (written to file)
      if (reconstruction.indel_filename != "None" && reconstruction.num_root_alignments == 1)
	{
	  if (reconstruction.loggingLevel >=1)
	    cerr<<"\nWriting indel information to file: " << reconstruction.indel_filename << endl; 
	  ofstream indel_file;
	  indel_file.open (reconstruction.indel_filename.c_str());
	  IndelCounter indels(annotated, &reconstruction.tree); 
	  indels.gather_indel_info(false); 
	  indels.display_indel_info(indel_file, reconstruction.per_branch);
	  indel_file.close();
	}
  
    }
  
  if (reconstruction.loggingLevel >= 1)
    {
      cerr<<"\nProtPal reconstruction completed without errors. \n";
    }
  }
  catch(const Dart_exception& e)
    {
      cerr<<e.what(); 
      exit(1); 
    }
  
  
  return(0);
}

