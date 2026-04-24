#n at the beginning of a sed script is equivalent to passing -n to sed.

# The start of a new filename is denoted by lines beginning with "FILE\t".
/^FILE\t/ {
	# Strip away the "FILE\t" at the beginning to leave just the filename.
	s|^FILE\t||
	# Append ".html" to the filename.
	s|$|.html|
	# Store it in the hold space.
	h
}

# Function and macro lines end with " ->"
/ ->$/ {
	# Function lines must contain parentheses.
	/(/ {
		# Strip away function arguments and that little arrow at the end.
		s|(.*||
		# Strip away the return type, leaving just the function name.
		s|[^ ]* ||
		# Add the filename stored in the hold space to the end of the pattern space, preceded by a newline.
		G

		# Print a comment line for easy consumption by toc.awk
		s|^|# |
		s|\n|()&|
		y|\n|\t|
		p
		y|\t|\n|
		s|()\(\n\)|\1|
		s|^# ||

		# Set \1 to the function name and \2 to the filename.
		# Replace the contents of the pattern space with a monster of a sed command.
		# The command replaces all instances of function() preceded by
		# a non-alphanumeric, non-underscore character with a link to that function.
		s|\(.*\)\n\(.*\)|s:\\([^a-zA-Z0-9_]\\)\1():\\1<a href=\2#\1><code>\1()</code></a>:g|
	}

	# Macro lines must not contain parentheses.
	/(/ ! {
		# Strip away the arrow at the end.
		s| .*||
		# Add the filename.
		G

		# Print a comment line for easy consumption by toc.awk
		s|^|# |
		y|\n|\t|
		p
		y|\t|\n|
		s|^# ||

		# Assemble the monster, but this time without matching for a pair of parentheses
		# and without matching function/macro lines.
		s|\(.*\)\n\(.*\)|/ ->$/!s:\\([^a-zA-Z0-9_]\\)\1\\([^A-Z0-9_]\\):\\1<a href=\2#\1><code>\1</code></a>\\2:g|
	}

	# Print out the monstrous sed command.
	p

	# Modify the command to match at the beginning of the line too.
	s|\\(\[^a-zA-Z0-9_\]\\)|^\\(\\)|
	s|\\1||
	# Print it.
	p

	# Modify the command to make things like <a href=#funcname> into <a href=file.html#funcname>
	s|.*<a href=\([^#]*\)#\([^>]*\).*|s:<a href=#\2>:<a href=\1#\2>:g|
	# Print it.
	p
}
