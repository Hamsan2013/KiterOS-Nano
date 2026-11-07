  app "Hello KiterOS"

when app starts:
    show "Welcome to KiterOS!"
end

remember score as 100

if score is greater than 50:
    show "Excellent!"
otherwise:
    show "Keep learning!"
end

create function greet:
    show "Hello from FRIDGE!"
end

call greet
