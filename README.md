### Volubilis Machinamentum
*"rotating mechanism"*

A physics-based puzzle game about machines created for [The Very Serious Juniper Dev Game Jam](https://itch.io/jam/theveryseriousjuniperdevgamejam),
where the theme was "spin to win".
Unfortunately I did not have enough time to finish the game so I did not submit it to the jam.
However, I have [uploaded it to itch](https://itshighnoon1.itch.io/volubilis-machinamentum).

In Volubilis Machinamentum, you build machines that have to do with rotation.
Or at least, that was the idea, but I did not have time to implement the levels I actually wanted to.
Some of the ideas were:
- Make a cart with spinning wheels
- Turn high pressure steam into spinning motion with a steam engine
- Transfer spinning motion with belts and gears
- Turn spinning motion into electricity with a generator
- Turn electricity into spinning motion with a motor
- Use a spinning motor and gears to create a clock
- Use a spinning motor to create an air compressor
The implementation of these levels would have required more components in the editor than just arbitrary shapes and bolts.
Like belts, hoses, valves, coils, a tool to create perfect circles/gears, etc.
Even without these things, I still found it fun to play around in the editor.

## Technology
I typically like to avoid libraries where possible, but I did use 3 technologies in this project.
All of them were new to me.

Volubilis Machinamentum is a C application that runs in the browser.
This is thanks to [Emscripten](https://emscripten.org/), which I [had been playing with](https://github.com/ItsHighNoon1/wasm-experiments) before working on this.
The best part of Emscripten is that I didn't even have to think about it, everything "just worked".
Minus some hiccups related to the filesystem not existing.
Emscripten is a 10/10 technology and it has fundamentally changed how I view the web as a deployment target.

The game features 2D physics with help from [Box2D](https://box2d.org/).
I found Box2D it easy to use though the documentation took some getting used it.
The most complicated part of using Box2D was working around the limits for polygon colliders, which required me to implement a triangulation algorithm.
Of course, "arbitrarily complex concave colliders" is pretty specific to my use case.
I will be looking for excuses to use Box2D in the future.
As an aside, I needed to compile Box2D from source to use it with Emscripten, which was pretty painless as there is an Emscripten build script.

Finally, as C does not have native support for pretty much anything, I was using [SDL3](https://wiki.libsdl.org/SDL3/FrontPage) for the frontend.
I chose SDL because it comes out-of-the-box with Emscripten, and given that I did not have time for a proper WebGL approach it was either use SDL or use the JavaScript linkage to drive a canvas.
SDL3 is fairly new and I unfortunately did not have a good time using it. Three reasons:
1. All of the resources online are for SDL2,
2. The details of this project just make rendering hard in general,
3. Weird interactions with Emscripten.
Admittedly, SDL2 would have absolutely been a good choice.
That's on me for not knowing at the outset.

## Retrospective
Honestly I'm feeling pretty defeated with this one. Sacrificed a lot of sleep to triangulation algorithms.
But you know what they say, failure is the greatest teacher, and it's better to fail after a week than a year.
So, where do I think I went wrong?

### Scope too big
When reading the list of ideas I had for levels, it's pretty clear that I wanted to do too much.
I got a bit attached to the idea of "a game where you have to make a steam engine".
Ok, that's fine, but there are so many things that you need for that:
- Physics simulation with axle joints which I had
- A way to draw straight lines
- An actual win condition
- Ok now I need some way to simulate with the concept of "high pressure steam" which includes valves
- Ok the average player won't be able to hook the valve up to the flywheel so probably I should give them [tag sensors](https://littlebigplanet.fandom.com/wiki/Tag_Sensor)
- Ok to connect the sensors to the valve there needs to be wires or something
which kind of starts to feel like too much scope.
Typically in this situation you start cutting scope, but

### A bad idea?
A good game idea is not necessarily a good game jam idea.
For example, the theme for this jam was "spin to win", and I made a physics simulation.
In order to incorporate the theme, I *have to* complete a significant amount of scope.
A game jammer's strongest tool is the ability to cut scope in order to meet the deadline.
But, there is no scope I can cut without losing the connection to the theme.
A good game jam idea has to be connected to the jam theme even in its simplest form, so that cutting scope later doesn't disqualify it.

### Prepare ahead of time
As I mentioned, this was my first time using Emscripten, Box2D, and SDL.
I had no experience whatsoever with Box2D and SDL before starting, and I was in the middle of playing with Emscripten when the jam started.
Sometimes, a jam is a good way to learn a technology (Box2D).
Sometimes, not knowing the technology ahead of time is a real hassle (SDL).
Having to take time to figure out what I was doing took away from time I could have been working on actual critical path development.
Since I am insistent on using my own game engine, there's something to be said for building the fundamentals before the jam even starts.
Which I have been known to do.

## Futures?
I think this is a cool idea and I'm on the fence about continuing to work on it.
Some more design work would be needed before I go back to development.
But this isn't really my top priority, it was a bit of an impulse decision to participate in this jam.
I'm glad I did because using Emscripten has me really optimistic about my future projects, but I think it's time for me to get back to engine development :).
