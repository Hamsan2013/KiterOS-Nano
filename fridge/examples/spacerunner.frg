  game "Space Runner"

remember score as 0

when game starts:
    clear "#060b19"
    create player
    set player position to 100, 200
    set player color to "#00e5ff"
    show "Tap canvas to boost player up!"
end

when player touches screen:
    move player up
    set score to score + 10
    show "Boost! Score: " + score
end
