**Note before reading: I wrote this about 5 years ago when I first started Uni and haven't done much development into it until recently, so bear with me on the explanations, I've lost a lot of the design choices and notes, and so I had to use quite a bit of AI to write some half sensible notes.**   

---

The engine is essentially a **discrete-event probabilistic football simulator**.
It models football as a sequence of micro-decisions resolved through attribute comparisons and randomness.

That is conceptually very similar to how Football Manager works internally, but this version is dramatically simplified, deterministic, and computationally lightweight.

---

# 1. Overall Architecture

The engine has four major systems:

1. **Data Layer**
2. **Simulation Core**
3. **Probability Resolution**
4. **Statistics + Ratings**

The flow is:

```
CSV Database
    ↓
Load Teams & Players
    ↓
Initialize Match State
    ↓
90-minute simulation loop
    ↓
Resolve thousands of micro-events
    ↓
Update stats + stamina
    ↓
Generate final match report
```

---

# 2. The Data Model

I model footballers using attributes.

Each player has:

```
Finishing
Heading
Attacking Position
Volleys
Penalties
Passing
Vision
Dribbling
First Touch
Crossing
Tackling
Marking
Interceptions
Strength
Aggression
Reflexes
Positioning
Composure
Anticipation
Pace
Stamina
Decisions
Work Rate
```

These mirror FM directly, but did not include all stats for the sake of simplicity.

FM also stores players as attribute vectors.

Example:

| Attribute   | Your Engine | FM |
| ----------- | ----------- | -- |
| Finishing   | ✔           | ✔  |
| Vision      | ✔           | ✔  |
| Decisions   | ✔           | ✔  |
| Positioning | ✔           | ✔  |
| Composure   | ✔           | ✔  |
| Pace        | ✔           | ✔  |

---

# 3. Why Attributes Matter

The engine turns football into:

```
Attacker skill
VS
Defender skill
```

Examples:

| Situation | Comparison               |
| --------- | ------------------------ |
| Shot      | Finishing vs Reflexes    |
| Pass      | Passing vs Interceptions |
| Dribble   | Dribbling vs Tackling    |

That is the fundamental core of most football simulation engines.

---

# 4. Match Flow

The simulation runs:

```cpp
for (minute = 1; minute <= 90; minute++)
{
    for (tick = 0; tick < 15; tick++)
    {
        resolve_event();
    }
}
```

So:

```
90 minutes × 15 ticks
= 1350 simulation events
```

Each event represents a tiny slice of football.

Examples:

* pass
* tackle
* dribble
* shot
* interception
* foul

This is similar to FM’s internal “match ticks”.

FM also runs from thousands to possibly millions of hidden simulation updates per match, though they could be exponentially more events running per tick

---

# 5. Possession System

I store:

```cpp
Team* possession;
```

At any moment:

```
home team has ball
OR
away team has ball
```

Actions can switch possession:

| Event           | Possession Changes? |
| --------------- | ------------------- |
| Successful pass | No                  |
| Interception    | Yes                 |
| Tackle          | Yes                 |
| Save            | Yes                 |
| Goal            | Yes                 |

This creates emergent match flow.

---

# 6. Event Selection

I randomly decide what the ball carrier attempts.

```cpp
double action_roll = roll(gen);
```

Then:

```cpp
if shot
else if dribble
else pass
```

This creates weighted football behavior. 
---

## Example

```cpp
if (action_roll > 0.94)
```

≈ 6% chance of a shot.

Strikers get boosted shot frequency:

```cpp
ball_carrier.position == "ST"
```

So attackers behave more aggressively.

FM does something similar using:

* role instructions
* mentality
* hidden tendencies

These could be simulated using [Utility AI](https://www.youtube.com/watch?v=p3Jbp2cZg3Q&pp=ygUKdXRpbGl0eSBhadIHCQkECwGHKiGM7w%3D%3D). An improvement would be adding [Behaviour Trees](https://www.youtube.com/watch?v=6VBCXvfNlCM&pp=ygUPYmVoYXZpb3VyIHRyZWVz).
I think these are used by FM in their simulations. 

---

# 7. The Probability Engine

This is the most important part.

You use:

```cpp
sigmoid_prob()
```

---

## Formula

I implemented the sigmoid function which is this formula:

$$
P=\frac{1}{1+e^{-k(a-d)}}
$$

Where:

* `a` = attacker attribute
* `d` = defender attribute
* `k` = steepness constant

---

# 8. Why Sigmoid Is Important

This creates realistic probabilities.

---

## Without sigmoid

If:

```
finishing > reflexes
```

attacker always wins.

That feels robotic.

---

## With sigmoid

A stronger player wins *more often*, not always.

Example:

| Difference | Success Chance |
| ---------- | -------------- |
| Equal      | 50%            |
| +10        | ~65%           |
| +20        | ~77%           |
| +40        | ~92%           |

This is extremely important.

FM also uses **probabilistic weighted** outcomes internally.

---

# 9. Shot Resolution

My shot system is layered.

---

## Phase 1: Shot Opportunity

```cpp
shot_lane_p
```

Represents:

* finding space
* beating defensive pressure
* avoiding blocks

Uses:

```
Finishing
VS
Anticipation
```

---

## Phase 2: Goalkeeper Duel

Then:

```
Reflexes + Positioning
VS
Finishing + Composure
```

This is realistic.

FM similarly resolves shots in stages:

1. chance creation
2. defensive pressure
3. shot quality
4. goalkeeper reaction

---

# 10. Passing System

Passing success:

```
Passing + Vision
VS
Interceptions
```

Long passes are harder:

```cpp
range_difficulty = 20
```

This is similar to FM’s hidden modifiers:

* pass distance
* pressure
* weather
* body orientation
* weak foot

---

# 11. Dribbling System

Dribble duel:

```
Dribbling + Pace
VS
Tackling + Strength
```

Failure can become:

* foul
* clean tackle

Again very FM-like conceptually.

---

# 12. Stamina System

I made it so that stamina reducing also degrades attributes over time.

```cpp
current_stamina -= 0.06
```

Then:

```cpp
get_eff(attr)
```

modifies performance levels (example long pass accuracy decreases with stamina).

Formula:

$$
A_{effective}=(0.8+0.2S)A
$$

Where:

* `A` = base attribute
* `S` = stamina %

So tired players become worse.

FM has a much more sophisticated fatigue model, but same principle.

---

# 13. Match Ratings

My ratings reward:

* goals
* saves
* tackles
* passing

And punish:

* failed passes
* missed shots
* fouls

FM also computes ratings from weighted event scoring.

---

# 14. Deterministic Simulation

I use:

```cpp
std::mt19937 gen(12345);
```

This means:

```
same seed = same match
```

This means I can:

* replay simulations
* debug balancing
* benchmark changes
* reproduce bugs

FM also uses seeded randomness internally, which is how matches can be replayed and be exactly the same.

---

# 15. Emergent Football

Even though my rules are simple, complex outcomes emerge:

* possession dominance
* high press effects
* elite strikers outperforming defenders
* good passers controlling matches
* strong keepers saving games

This is the magic of simulation systems.

Simple local rules create believable macro behavior.

---

# 16. Biggest Differences From FM

This engine is far simpler.

FM includes:

| Feature              | FM | Yours |
| -------------------- | -- | ----- |
| Spatial coordinates  | ✔  | ✘     |
| Player movement maps | ✔  | ✘     |
| Tactical shapes      | ✔  | ✘     |
| Formation geometry   | ✔  | ✘     |
| Ball trajectory      | ✔  | ✘     |
| Physics engine       | ✔  | ✘     |
| Role AI              | ✔  | ✘     |
| Pressing systems     | ✔  | ✘     |
| Tactical familiarity | ✔  | ✘     |
| Morale               | ✔  | ✘     |
| Weather              | ✔  | ✘     |
| Dynamic mentality    | ✔  | ✘     |
| Animation engine     | ✔  | ✘     |

My engine is:

```
event-based
```

FM is:

```text
spatial + event hybrid
```

---

# 17. What FM Probably Does Internally

FM likely has:

```text
player positions
+
continuous movement
+
event resolution
+
probability systems
```

My engine skips the geometry and simulates only outcomes.

That makes it:

* MUCH easier
* MUCH faster
* easier to debug
* easier to expand

But:
* Mainly text-based
* less exciting to sim games
* not as complex to play

---

# 18. Why the Engine Is a Good Base

My design already has:

* probabilistic realism
* attribute interactions
* stamina decay
* emergent gameplay
* deterministic reproducibility
* layered event resolution
* team statistics
* player statistics

Which is already the foundation of a legitimate football simulation engine.

---

# 19. How I Could Evolve It Toward FM

The next major upgrades would be:

---

## A. Spatial Simulation

Adding:

```cpp
x, y positions
```

Then:

* distance-based passing
* defensive shape
* space creation
* through balls

This could be a huge leap to making the match engine feel good
---

## B. Tactical Systems

Add:

* formations
* pressing intensity
* defensive line
* tempo
* width

These could modify event probabilities, by changing the specific situations that could happen (Example: a tactic could prioritize making wingers take on fullbacks more, so dribbling events happen more with higher probabilities, like with a player like Yamal)
---

## C. Role-Based AI

Example:

| Role      | Behavior      |
| --------- | ------------- |
| Poacher   | shoots more   |
| Playmaker | passes more   |
| Winger    | dribbles more |

FM heavily relies on role logic, this would probably be where the Behavior Tree AI would be most used.

---

## D. Dynamic Match Momentum

Add hidden confidence/momentum variables.

Real football has psychological swings, home advantage, last minute chances, etc.

FM might be modelling this.

---

# 20. Final Summary

The engine captures the *core abstraction* of football simulation:

```
Football =
continuous probabilistic contests
between player attributes
under changing match conditions
```

That is fundamentally the same philosophy as FM.

The difference is:

| Your Engine   | FM                 |
| ------------- | ------------------ |
| abstract      | spatial            |
| lightweight   | enormous           |
| event-driven  | hybrid simulation  |
| deterministic | semi-deterministic |
| thousands LOC | millions LOC       |

This is probably just the skeleton of a real football simulation engine.
