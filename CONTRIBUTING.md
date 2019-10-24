Contributing to miniaudio
=========================

Found a Bug?
------------
If you've found a bug you can create a bug report [here on GitHub](https://github.com/dr-soft/miniaudio/issues).
The more information you can provide, the quicker I'll be able to get it fixed. Sample programs and files help a
lot, as does a detailed list of steps I can follow to reproduce the problem.

You can also submit a pull request which, provided your fix is correct and well written, is the quickest way to
get the bug fixed. See the next section for guidance on pull requests.


Pull Requests
-------------
If you want to do actual development on miniaudio, pull requests are the best place to start. Just don't do any
significant work without talking to me first. If I don't like it, it won't be merged. Always base your pull
request branch on the "dev" branch. The master branch contains the latest release, which means your pull request
may not be including the lastest in-development changes which may result in unnecessary conflicts.

I need to review your pull requests before merging. If your pull request is non-trivial, try to break it up into
logical bite sized commits to make it easier for review, but make sure every commit compiles.

When it comes to coding style I'm fairly relaxed, but be a professional and respect the existing coding style,
regardless of whether or not you like it. It's no big deal if something slips, but try to keep it in mind. Some
things in particular:
  * C89. `/*...*/` style comments and variables declared at the top of the code block are the main thing.
  * Spaces instead of tabs. 4 spaces per tab.
  * Don't add a third party dependency. If you do this I'll immediately reject your pull request.

I'm not going to outline specific coding styles - just look at the existing code and use common sense.


Licensing and Credits
---------------------
miniaudio is dual licensed as a choice of public domain or MIT-0 (No Attribution), so you need to agree to release
your contributions as such. I also do not maintain a credit/contributions list. If you don't like this you should
not contribute to this project.


Before You Ask:
---------------
* No, I'm not switching away from C89.
* No, I'm not adding a third license option.
* No, I'm not adding you to any kind of credit list.
* No, I'm not splitting the project out into multiple files.
* No, I'm not adding support for CMake nor any other kind of build system.
* No, there's no Code of Conduct and I'm not adding one. Just don't be unpleasant.