# LetterBoxed

## Solver for NYT Word Puzzle Letter Boxed
* [Game Link](https://www.nytimes.com/puzzles/letter-boxed)
* [Tutorial](https://wordfinder.yourdictionary.com/blog/nyts-letter-boxed-a-quick-guide-to-the-fan-favorite-puzzle/)
* [Letterboxed Answers](https://letterboxedanswers.com) is a good source for test puzzles
* Regardless of the number of target words specified in the instructions for a given day there is always a two word solution.

## Build
* ```gen_project.sh```
    * generates project via cmake
    * builds project
    * runs tests

## Usage
* letterboxed [side1] [side2] [side3] [side4]
    e.g. letterboxed vrq wue isl dmo
* Produces list of all potential two word solutions sorted shortest to longest

## Third Party Resources
* [words_alpha.txt](https://github.com/dwyl/english-words)
