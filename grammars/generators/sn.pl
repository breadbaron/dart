#!/usr/bin/perl -w

# flag indicating whether to use codon or nucleotide frequencies in mutation rates
# for now we use codon frequencies, as this is most similar to CodeML
#  bear in mind that using codon frequencies instead of base frequencies, but keeping s & n fixed,
#  effectively gives you rates that are lower (by a factor that I hazily estimate at around 48 = 3*4*4)
my $useCodonFreqs = 1;

# alphabet
my @alph = qw(a c g t);

# transition & transversion tables
my %transition = qw (a g
		     c t
		     g a
		     t c);

my %transversions;
for my $a (@alph) {
    $transversions{$a} = [ grep ($_ ne $a && $_ ne $transition{$a}, @alph) ];
}

# codon table
my %aa = qw( ttt F  tct S  tat Y  tgt C
	     ttc F  tcc S  tac Y  tgc C
	     tta L  tca S  taa X  tga X
	     ttg L  tcg S  tag X  tgg W
	     
	     ctt L  cct P  cat H  cgt R
	     ctc L  ccc P  cac H  cgc R
	     cta L  cca P  caa Q  cga R
	     ctg L  ccg P  cag Q  cgg R
	     
	     att I  act T  aat N  agt S
	     atc I  acc T  aac N  agc S
	     ata I  aca T  aaa K  aga R
	     atg M  acg T  aag K  agg R
	   
	     gtt V  gct A  gat D  ggt G
	     gtc V  gcc A  gac D  ggc G
	     gta V  gca A  gaa E  gga G
	     gtg V  gcg A  gag E  ggg G );

# flag: print mutations to self?
my $toSelf = 1;

# usage
my $usage = "Usage: $0 [-noself] <template file>\n";
my @argv;
while (@ARGV) {
    my $argv = shift;
    if ($argv =~ /^-h/) { die $usage }
    elsif ($argv eq "-noself") { $toSelf = 0 }
    elsif ($argv =~ /^-\S/) { die $usage }
    else { push @argv, $argv }
}
die $usage if @argv != 1;

# get args: path to template
my ($tmpl_path) = @argv;

# load template
open TMPL, "<$tmpl_path" or die $!;
my @tmpl = <TMPL>;
close TMPL;
my $tmpl = join "", @tmpl;

# make rates
my (@initial, @mutate);
for my $ijk (sort keys %aa) {
    my @ijk = split //, $ijk;
    my ($i, $j, $k) = @ijk;
    for my $x (@alph) {
	if ($x ne $i) { push @mutate, mutate ($ijk, "$x$j$k") }
	if ($x ne $j) { push @mutate, mutate ($ijk, "$i$x$k") }
	if ($x ne $k) { push @mutate, mutate ($ijk, "$i$j$x") }
    }
    push @mutate, mutate ($ijk, $ijk) if $toSelf;
    push @initial, "(initial (state ($i $j $k)) (prob " . join(" * ",map(p_branch_var($ijk[$_],$_),0..2)) . "))  ;; $aa{$ijk}" unless $aa{$ijk} eq "X";
}


# substitute into template
my $mutPattern = '(\s+)[^\n]*==MUTATE\((\S+?)\)==';
my $nodeVar = node_var();
while ($tmpl =~ /$mutPattern/) {
    my ($spc, $arg) = ($1, $2);
    my $mutate = join ("", map ("$spc$_", @initial, @mutate));
    $mutate =~ s/$nodeVar/$arg/g;
    $tmpl =~ s/$mutPattern/$mutate/;
}

# print
print $tmpl;

# create mutate expression
sub mutate {
    my ($ijk, $xyz) = @_;
    my ($i, $j, $k) = split //, $ijk;
    my ($x, $y, $z) = split //, $xyz;
    my ($rate, $comment) = rateFunc($ijk,$xyz);
    return
	defined($rate)
	? ("(mutate (from ($i $j $k)) (to ($x $y $z)) (rate ($rate)))  ;; $comment")
	: ();
}

# get rate for a given mutation
sub rateFunc {
    my ($ijk, $xyz) = @_;

    return (undef,undef) if $aa{$ijk} eq "X" || $aa{$xyz} eq "X";

    my ($rFunc, $comment);
    my $s = s_branch_var();
    my $n = n_branch_var();
    my $k = k_branch_var();
    my $not_k = not_k_branch_var();
    if ($ijk eq $xyz) {
	my (@s, @n, @s_notk, @n_notk);
	my @ijk = split //, $ijk;
	for my $pos (0..2) {
	    my $ijk_pos = substr ($ijk, $pos, 1);
	    my $ijk_left = join ("", @ijk[0..$pos-1]);
	    my $ijk_right = join ("", @ijk[$pos+1..2]);
	    die unless $ijk eq $ijk_left . $ijk_pos . $ijk_right;

	    my @ijk_left_pvar = map (inert(p_branch_var($ijk[$_],$_)), 0..$pos-1);
	    my @ijk_right_pvar = map (inert(p_branch_var($ijk[$_],$_)), $pos+1..2);

	    my @mut_pos = grep ($_ ne $ijk_pos, @alph);
	    my %mut_aa = map (($_ => $aa{"${ijk_left}${_}${ijk_right}"}), @mut_pos);

	    my @nonsyn_xyz_pos = grep ($aa{$ijk} ne $mut_aa{$_} && $mut_aa{$_} ne "X", @mut_pos);  # don't count stop codons
	    my @nonsyn_transversions = grep ($_ ne $transition{$ijk_pos}, @nonsyn_xyz_pos);

	    my @syn_xyz_pos = grep ($aa{$ijk} eq $mut_aa{$_}, @mut_pos);
	    my @syn_transversions = grep ($_ ne $transition{$ijk_pos}, @syn_xyz_pos);

	    my (@nonsyn_transversion_pvars, @syn_transversion_pvars);
	    if ($useCodonFreqs) {
		@nonsyn_transversion_pvars = map ("(" . join (" * ", @$_) . ")", map ([@ijk_left_pvar, inert(p_branch_var($_,$pos)), @ijk_right_pvar], @nonsyn_transversions));
		@syn_transversion_pvars = map ("(" . join (" * ", @$_) . ")", map ([@ijk_left_pvar, inert(p_branch_var($_,$pos)), @ijk_right_pvar], @syn_transversions));
	    } else {
		@nonsyn_transversion_pvars = map (inert(p_branch_var($_,$pos)), @nonsyn_transversions);
		@syn_transversion_pvars = map (inert(p_branch_var($_,$pos)), @syn_transversions);
	    }

	    push @n_notk, @nonsyn_transversion_pvars;
	    push @s_notk, @syn_transversion_pvars;
	}

	$rFunc = "$not_k * ("
	    . join (" + ",
		    (@s_notk==0 ? () : (inert($s) . " * (" . join (" + ", @s_notk) . ")")),
		    (@n_notk==0 ? () : (inert($n) . " * (" . join (" + ", @n_notk) . ")")))
	    . ")";

	$comment = "unobserved rejected transversions";

    } else {
	my $nDiff = 0;
	my ($isTransition, $pFunc);
	my @pFunc;
	for my $pos (0..2) {
	    my $ijk_pos = substr ($ijk, $pos, 1);
	    my $xyz_pos = substr ($xyz, $pos, 1);
	    if ($ijk_pos eq $xyz_pos) {
		if ($useCodonFreqs) {
		    push @pFunc, p_branch_var ($xyz_pos, $pos);
		}
	    } else {
		$isTransition = $transition{$ijk_pos} eq $xyz_pos;
		push @pFunc, p_branch_var ($xyz_pos, $pos);
		++$nDiff;
	    }
	}

	return (undef,undef) unless $nDiff == 1;

	my $isSynonymous = $aa{$ijk} eq $aa{$xyz};
	$rFunc = ($isSynonymous ? s_branch_var() : n_branch_var())
	    . " * "
	    . join (" * ", @pFunc)
	    . ($isTransition ? "" : " * $k");
	$comment =
	    "$aa{$ijk} -> $aa{$xyz} (" .
	    ($isSynonymous ? "" : "non") .
	    "synonymous " .
	    ($isTransition ? "transition" : "transversion") .
	    ")";

    }

    return ($rFunc, $comment);
}

# branch-specific variable name generator
sub node_var { return "NODE" }
sub branch_var { my ($var) = @_; return "(. ${var}_ " . node_var() . ")" }

sub inert { return "(# " . shift() . ")" }
sub s_branch_var { return branch_var("s") }
sub n_branch_var { return branch_var("n") }
sub p_branch_var { my ($new, $pos) = @_; return branch_var ("p$new$pos") }
sub k_branch_var { return branch_var("k") }
sub not_k_branch_var { return branch_var("~k") }
