# Escape HTML.
s|&|\&amp;|g
s|<|\&lt;|g
s|>|\&gt;|g

/^diff --git / {
	s|^|<b>|

	# Set the hold space to "1".
	x
	s|.*|1|
	x
}

/^@@ / {
	s|$|</b>|

	# Clear the hold space.
	x
	s|.*||
	x
}

# Mark insertions and deletions, but only if there is no "1" in the hold space (i.e., only if not between "^diff --git " and "^@@ ".
x
/^$/ {
	x
	s|^-.*|<del>&</del>|
	s|^+.*|<ins>&</ins>|
	x
}
x

# Add <pre> and such and command line to start.
1s|\(.*\)\t\(.*\)\t\(.*\)|<details><summary>\3 commit \2</summary><pre><samp>\&gt; </samp><kbd>git clone '\1' tmpdir \&amp;\&amp; \\\n\tcd tmpdir \&amp;\&amp; \\\n\tgit show \2\n|

# Truncate <summary> commit
1s|\( commit .........\)[^<]*|\1|

# Close it out at the start of the 2nd line.
2s|^|</kbd><samp>|

# Close the <pre> (and such).
$s|$|</samp></pre></details>|