local sync = 0
for i =0,7 do
setParam(i, 20, 20)
setParam(i, 21, 128)
setParam(i, 44, 0)
setParam(i, 45, 18)
setParam(i, 12, 120)

end
setParam(3, 20, 48)
setParam(3, 21, 188)

setParam(3, 12, 100)
setParam(3, 13, 60)

chord = {0,3,5,0,3,5,0,3,7}
chordI = 1

c2 = {0,7,5,12+3}
c2I = 1

function tempoSync()
    sync = sync+1
    if (sync%(96*5) == 0) then
        c2I = c2I+1
        if(c2I>4) then c2I = 1 end
        playNote(1, 60-12+chord[chordI]+c2[c2I])
        setParam(1, 12, 180+math.floor(math.random(-60, 60)))
        playNote(3, 60-12+7+chord[chordI]+c2[c2I])
        setParam(3, 12, 180+math.floor(math.random(-60, 60)))
    end
    if sync%(96) == 0 then
        chordI = chordI+1
        if chordI > 9 then chordI = 1 end
        playNote(0, 60+chord[chordI]+c2[c2I])
        setParam(0, 12, 180+math.floor(math.random(-60, 60)))
    end
    if sync%(96/6*11) == 0 then
        playNote(2, 60+12+chord[chordI]+c2[c2I])
        setParam(2, 12, 180+math.floor(math.random(-60, 60)))
        if math.random() > 0.9 then setParam(2, 44, 128) else setParam(2, 44, 0) end
    end
end
