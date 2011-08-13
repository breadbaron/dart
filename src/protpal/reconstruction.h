#ifndef RECONSTRUCTION_H
#define RECONSTRUCTION_H

#include<iostream>
#include<string>
#include<map>

#include "protpal/utils.h"
#include "protpal/profile.h"
#include "protpal/transducer.h"
#include "protpal/AlignmentEnvelope.h"
#include "protpal/ReadProfileScore.h"
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
  MyMap<string, string> options; 

  // Get sequences
  void parse_sequences(Alphabet); 
  
  // Each leaf node is assigned a sequence. Possibly delete these after making ExactMatch transducers for each
  // leaf.  Hmm, maybe I should have used pointers instead. 
  MyMap<string, string> sequences; 
  sstring truncate_names_char; //truncate the names by this character, if requested

  // not (yet) using Ian's fancy Alphabet class
  // at least this is not a string any more, but a vector of strings, allowing latent-variable models.
  //  vector<string> alphabet; 

  // rate matrix and sequence import
  bool use_dna; 
  bool codon_model; 
  sstring codon_matrix_filename; 
  sstring rate_matrix_filename;   
  sstring grammar_filename;   
  sstring gap_grammar_filename;   
  sstring stkFileName;
  sstring fastaFileName;
  sstring root_profile_filename; 
  bool root_viterbi_path; 
  sstring indel_filename;   
  sstring db_filename;   
  sstring treeFileName; 
  void verify_leaf_sequences(void); 
  // Alignment envelope stuff
  sstring guide_alignment_filename; 
  int guide_sausage; 
  AlignmentEnvelope envelope; 
  sstring envelope_type; 

  string sequenceFileType; 

  // the phylogenetic tree, stored as a PHYLYP_tree object.  This is a fairly hairy object, with most of its code
  // living in phylogeny.*
  sstring treeString;
  PHYLIP_tree tree; 


  // Each tree node will at some point contain a sequence profile.  The profile at a node  is constructed via its
  // two children, which are viciously deleted after the parent is constructed.  
  MyMap<node, AbsorbingTransducer> profiles; 


  // default naming of internal nodes.
  void set_node_names(void);
  
  // simulation
  bool simulate; 
  void make_sexpr_file(ostream&);
  void show_branch(node, ostream&); 
  void simulate_alignment(void);
  int rootLength; 
  sstring phylocomposer_filename; 
  
  //simulation utility functions
  Digitized_biosequence sample_pairwise(Digitized_biosequence, BranchTrans, Node, Node, Decomposition&); 
  Digitized_biosequence sample_root(SingletTrans); 
  
  // show indel'd events
  MyMap<node, Profile> pre_summed_profiles; 
  
  // Reconstruction algorithm parameters

  Alphabet alphabet;
  Irrev_EM_matrix rate_matrix; 
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
  bool stoch_trace, viterbi, input_alignment, train_grammar; 
  bool estimate_root_insert;   
  bool per_branch;
  //model parameters
  bool mixture_2; 
  bool mixture_3; 
  double ins_rate;
  double del_rate;   
  double sub_rate;
  double max_branch_length; 
  double gap_extend; 
  double gap_extend_2; 
  double mix_prior_2;
  double gap_extend_3; 
  double mix_prior_3;
  double root_insert_prob;
  sstring gap_char; 
  
  // Indel investigation
  int num_root_alignments; 
  bool viterbi_alignments; ; 
  double get_root_ins_estimate(void);  

  // Placement algorithm
  sstring reads_to_place_filename;
  sstring saved_profiles_directory; 
  sstring saved_subtree_profiles_directory; 
  sstring json_placements_filename; 
  bool no_placements_tabular; 
  map<int, string> check_profile_filenames(void);
  map< pair<node, string>, bool> init_limiter(void); 
  sstring profile_to_make; 
  void write_placement_JSON(ostream& out,ScoreMap& scores); 
  void write_placement_tabular(ostream& out,ScoreMap& scores); 
  void parse_placement_tabular(ScoreMap& scores); 
  sstring score_tabular_filename; 
  void write_numbered_newick(ostream& out, bool quotes=true); 
  
  
  // Memory management
  void clear_child(node);

  // newly made  public
  void get_tree_from_file(const char*);

 private:
  // Help message
  void display_opts(void);
  
  // loading data 
  bool have_tree;
  bool have_sequences;   


  void load_rate_matrix(const string);
  void loadTreeString(const char*);
  void get_stockholm_tree(const char*);  

  
};
#endif
