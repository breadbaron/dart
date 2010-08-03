#ifndef RECONSTRUCTION_H
#define RECONSTRUCTION_H

#include<iostream>
#include<string>
#include<map>

#include "protpal/utils.h"
#include "protpal/profile.h"
#include "protpal/transducer.h"
#include "tree/phylogeny.h"
#include "ecfg/ecfgsexpr.h"
#include "seq/biosequence.h"
#include "seq/alignment.h"

class Reconstruction
{
 public:
  // Constructor  - options are parsed from cmd line
  Reconstruction(int argc, char* argv[]);

  // store the possible cmd line options, and brief descriptions
  map<string, string> options; 

  // Get sequences
  void parse_sequences(Alphabet); 
  
  // Each leaf node is assigned a sequence. Possibly delete these after making ExactMatch transducers for each
  // leaf.  Hmm, maybe I should have used pointers instead. 
  map<string, string> sequences; 

  // not (yet) using Ian's fancy Alphabet class
  // at least this is not a string any more, but a vector of strings, allowing latent-variable models.
  vector<string> alphabet; 

  // rate matrix and sequence import
  sstring rate_matrix_filename;   
  sstring grammar_filename;   
  sstring stkFileName;
  sstring fastaFileName;
  sstring indel_filename;   
  sstring treeFileName; 
  
  string sequenceFileType; 

  // the phylogenetic tree, stored as a PHYLYP_tree object.  This is a fairly hairy object, with most of its code
  // living in phylogeny.*
  sstring treeString;
  PHYLIP_tree tree; 

  // Each tree node will at some point contain a sequence profile.  The profile at a node  is constructed via its
  // two children, which are viciously deleted after the parent is constructed.  
  map<node, AbsorbingTransducer> profiles; 


  // default naming of internal nodes.
  void set_node_names(void);
  
  // simulation
  bool simulate; 
  void make_sexpr_file(Alphabet, Irrev_EM_matrix);
  string show_branch(node); 
  void simulate_alignment(Alphabet, Irrev_EM_matrix);
  int rootLength; 
  
  //simulation utility functions
  Digitized_biosequence sample_pairwise(Digitized_biosequence, BranchTrans, Node, Node, Decomposition&); 
  Digitized_biosequence sample_root(SingletTrans); 
  
  // show indel'd events
  void show_indels(Stockholm); 
  
  // Reconstruction algorithm parameters
  int num_sampled_paths;
  int max_sampled_externals; 
  bool show_alignments; 
  int loggingLevel;
  int envelope_distance; 
  bool leaves_only; 
  bool ancrec_postprob; 
  double min_ancrec_postprob; 
  bool xrate_output; 
  bool fasta_output; 
  bool clock_seed; 
  bool estimate_params; 

  
  //model parameters
  double ins_rate;
  double del_rate;   
  double gap_extend; 

 private:
  // Help message
  void display_opts(void);
  
  // loading data 
  bool have_tree;
  bool have_sequences;   
  
  void load_rate_matrix(const string);
  void get_tree_from_file(const char*);
  void loadTreeString(const char*);
  void get_stockholm_tree(const char*);  
};
#endif