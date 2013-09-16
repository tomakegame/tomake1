package Pattern::RE_Type2;

use strict;

use Pattern;
use base qw(Pattern);
use BaseFile;
use ExtractorGen;
use Globals qw($extractor $map_found);

####################################
### CATEGORY: Constructor
####################################

##
# Pattern->new()
#
# Create a new Pattern object.
sub new {
	my $class = shift;
	my %args = @_;
	my $self = $class->SUPER::new(@_);
	
	$self->{map_pointer_pattern} = "8B ?? 89 ?? ?? ?? E8 ?? ?? ?? ?? C7 44 24 14 ?? ?? ?? ?? 8B ?? E8 ?? ?? ?? ?? C7 44 24 14"; # Pattern to find Function Pointer
	$self->{map_pointer_entry_offset} = 22; # Offset in Pattern to get the
	
	bless $self, $class;

	return $self;
}

####################################
### CATEGORY: Public
####################################

# void $Pattern->onFound(int offset, BaseFile base_file)
#
# Event Handler to process the $base_file if the patter was found
#
# Overload it in Children
sub onProcess {
	my ($self, $offset, $base_file) = @_;
	
	$map_found = 1;
	print "Packet Len map found. Type: RE #2\n";
	
	$extractor = new ExtractorGen($base_file, $offset);
	$extractor->generate_extractor();
	printf "Packet Len Map offset: %i (%0.8X)-> %0.8X\n", $offset, $offset, $offset + 0x400000;
	return 1;
};


1;