#ifndef READPROFILESCORE_H
#define READPROFILESCORE_H
#include<iostream>

#include "protpal/utils.h"
#include "protpal/profile.h"
#include "util/sstring.h"
#include "ecfg/ecfgsexpr.h" // for array2d class
#include "seq/biosequence.h" // for Weight_profile


class Read : public list<sstring>
{
 public:
  sstring identifier; 
  sstring sstringRep; 
  void pad(void); 
  void set(sstring); 
};

typedef pair<state, state> compositeState; 
typedef map<string, map<string, bfloat> > ScoreMap; // map from reads to tree nodes to placement likelihoods

class ReadProfileModel
{
 public:
  // Constructor
  ReadProfileModel(void);

  // Profile - one emission side
  AbsorbingTransducer *profile; 

  // Substitution model
  Alphabet sub_alphabet;
  int alphabet_size; 
  Irrev_EM_matrix rate_matrix; 
  double branch_length; 

  // Set up substitution model
  void set_substitution_model(Alphabet&, Irrev_EM_matrix&, AbsorbingTransducer*);
  array2d<double> conditional_sub_matrix;
  vector<double> equilibrium_dist; 
  
  // Some variables particular to the emission weight function
  bfloat emissionWeight; 
  int alphIdx; 
  Symbol_weight_map::iterator symbolIter; 

  // Basic info
  int num_states; 
  state start_state, end_state; 
  Weight_profile read_profile; 

  // Map state names to indices and reverse - allows access to states by name in functions
  map<string, int> state_index; 
  vector<string> state_name; 

  // State types = start, end, insert, delete, match.  Arbitrarily defined such that "insert" 
  // emits only to the profile (and not the read).  
  map<state, string> state_type; 

  // Building up the HMM by its states and transitions.  Eventually this might be done by parsing a
  // .SExpr file, though it's built-in for now...
  void add_state(string type, string name); 
  void add_transition(string from, string to, bfloat weight); 

  // Transition connectivity and the associated weights
  map<pair<state, state>, bfloat> transition; 
  inline bfloat get_transition_weight(state,state); 
  map<state, list<state> > incoming; 
  
  // Emission weight - the likelihood of emitting a read character matched to a profile state
  bfloat get_emission_weight(int readCharIndex, state profileState, state hmmState); 
  bfloat get_emission_weight_by_alphabet_index(int alphIndex, state profileState, state hmmState); 

  // Non-special states, not start or end
  list<state> non_special_states; 
  
  
  // Extra variables  
  pair< state, state> transitionPair; 
  map<pair<state, state>, bfloat>::iterator tmpIter; 
};


class ReadProfileScore
{
  // Main class to allow "scoring" a read to a profile, via a  "pair HMM sum-over-alignments" likelihood
 public:
  // Constructor
  ReadProfileScore(AbsorbingTransducer *prof_in, Alphabet&, Irrev_EM_matrix&);

  // Main wrapper functions - get the likelihood score of a read to a profile, and print it to an ostream or store it
  // for later calculations
  void score_and_print(const Read& read, ostream& out, bool viterbi=false);
  void score_and_store(const Read& read , ScoreMap& scores, bool viterbi=false);

  // Main workhorse function
  bfloat get_score(const Read&, bool viterbi, bool logging=false); 

  // Name and read will change upon analyzing different reads/profiles.  
  string name;
  Read read; 
  void HMMoC_adapter(const char* filename, bool precompute=true); 

  // Extra variables
  int profSize, readSize; 

  // Alphabet and rate matrix
  Alphabet sub_alphabet; 
  Irrev_EM_matrix sub_rate_matrix; 

  // public for now...
  void fill_DP_matrix(const Read& read, ostream& hmmoc, bool hmmoc_only=false, bool backPointers=false, bool logging=false); 
  void clear_DP_matrix(void);
 private:
  // Profile - one emission side
  AbsorbingTransducer *profile; 

  // profile -> read maps

  // The states of this profile that are relevant
  vector<state> profile_states;

  // Pair HMM that directs the alignment calculation
  ReadProfileModel pairHMM; 

  // Call up results after DP
  inline bfloat get_forward_value(void); 
  bfloat get_viterbi_value(void); 
  
  // Viterbi traceback requires pointers to be stored during DP
  inline void add_backPointer(int, state, state, int, state, state); 
  map< vector<int>, vector<int> > backPointers; 
  vector<int> pointer_ID;   


  // Write the result to file
  void write_read_info(ostream& out, const Read& read, bfloat value); 

  // Manage the dynamic programming matrix
  inline bfloat get_DP_cell(int,int,int); 
  inline void set_DP_cell(int,int,int, bfloat); 
  inline void add_to_DP_cell(int,int,int, bfloat); 
  map< vector<int>, bfloat> DP_matrix; 
  vector<int> DP_ID; 

  // Helper functions for DP recursions - get the incoming states based on current state, etc
  inline int get_incoming_read_state(int readIndex, state hmm_state);
  inline vector<state> get_incoming_profile_states(state profileState, state hmm_state);
  inline list<state> get_possible_HMM_states(state readState, state profileState);
  bfloat hmm_transition_weight; 



  // HMMoC adapter stuff
  void add_state_to_HMMoC(ostream&, state toProfile, state toHMM, state fromProfile, state fromHMM, bool logging=true ); 
  vector<compositeState > states_added; 
  vector<pair<compositeState, compositeState > > transitions_added; 
  vector<vector<compositeState> > cliques; 
  string composite2string(compositeState);
  vector<string> start_clique, mainClique, end_clique;
  stringstream transitionStream; 
  vector<bfloat> probValues; 
  string alphabet; 
  };


#endif
