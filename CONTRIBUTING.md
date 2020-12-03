Contributing to miniaudio
=========================
First of all, thanks for stopping by! This document will explain a few things to consider when contributing to
miniaudio.


Found a Bug?
------------
If you've found a bug you can create a bug report [here on GitHub](https://github.com/mackron/miniaudio/issues).
The more information you can provide, the quicker I'll be able to get it fixed. Sample programs and files help a
lot, as does a detailed list of steps I can follow to reproduce the problem.

You can also submit a pull request which, provided your fix is correct and well written, is the quickest way to
get the bug fixed. See the next section for guidance on pull requests.


Pull Requests
-------------
If you want to do actual development on miniaudio, pull requests are the best place to start. Just don't do any
significant work without talking to me first. If I don't like it, it won't be merged. You can find me via email,
[Discord](https://discord.gg/9vpqbjU) and [Twitter](https://twitter.com/mackron).

Always base your pull request branch on the "dev" branch. The master branch contains the latest release, which
means your pull request may not be including the lastest in-development changes which may result in unnecessary
conflicts.

I need to review your pull requests before merging. If your pull request is non-trivial, try to break it up into
logical bite sized commits to make it easier for review, but make sure every commit compiles. Obviously this is
not always easy to do in practice, but just keep it in mind.

When it comes to coding style I'm fairly relaxed, but be professional and respect the existing coding style,
regardless of whether or not you like it. It's no big deal if something slips, but try to keep it in mind. Some
things in particular:
  * C89. `/*...*/` style comments and variables declared at the top of the code block are the main thing.
  * Spaces instead of tabs. 4 spaces per tab.
  * Don't add a third party dependency. If you do this I'll immediately reject your pull request.

I'm not going to outline specific coding styles - just look at the existing code and use common sense.

If you want to submit a pull request for any of the dr_* libraries in the "extras" folder, please submit the pull
request to the [dr_libs repository](https://github.com/mackron/dr_libs).


Respect the Goals of the Project
--------------------------------
When making a contribution, please respect the primary goals of the project. These are the points of difference
that make miniaudio unique and it's important they're maintained and respected.

  * miniaudio is *single file*. Do not split your work into multiple files thinking I'll be impressed with your
    modular design - I won't, and your contribution will be immediately rejected.
  * miniaudio has *no external dependencies*. You might think your helping by adding some cool new feature via
    some external library. You're not helping, and your contribution will be immediately rejected.
  * miniaudio is *public domain*. Don't add any code that's taken directly from licensed code.
  


Licensing and Credits
---------------------
miniaudio is dual licensed as a choice of public domain or MIT-0 (No Attribution), so you need to agree to release
your contributions as such. I also do not maintain a credit/contributions list. If you don't like this you should
not contribute to this project.


Predictable Questions
---------------------
### "Would you consider splitting out [some section of code] into it's own file?"
No, the idea is to keep everything in one place. It would be nice in specific cases to split out specific sections
of the code, such as the resampler, for example. However, this will completely violate one of the major goals of the
project - to have a complete audio library contained within a single file.

### "Would you consider adding support for CMake [or my favourite build system]?"
No, the whole point of having the code contained entirely within a single file and not have any external dependencies
is to make it easy to add to your source tree without needing any extra build system integration. There is no need to
incur the cost of maintaining build systems in miniaudio.

### "Would you consider feature XYZ? It requires C11, but don't worry, all compilers support it."
One of the philosophies of miniaudio is that it should just work, and that includes compilation environment. There's
no real reason to not support older compilers. Newer versions of C will not add anything of any significance
that cannot already be done in C89.

### "Will you consider adding a third license option such as [my favourite license]?"
No, the idea is to keep licensing simple. That's why miniaudio is public domain - to avoid as much license friction
as possible. However, some regions do not recognize public domain, so therefore an alternative license, MIT No
Attribution, is included as an added option. There is no need to make the licensing situation any more confusing.

### "Is there a list of contributors? Will my name be added to any kind of list?"
No, there's no credit list as it just adds extra maintenance work and it's too easy to accidentally and unfairly
forget to add a contributor. A list of contributors can be retrieved from the Git log and also GitHub itself.
