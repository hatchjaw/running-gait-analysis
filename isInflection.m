function result = isInflection(buffer, mode)
    switch mode
        case 'min'
            result = (sign(buffer(1)) == 1 && sign(buffer(2)) == -1) || ...
            (sign(buffer(1)) == 1 && sign(buffer(2)) == 0 && sign(buffer(3)) == -1);
        otherwise % max
            result = (sign(buffer(1)) == -1 && sign(buffer(2)) == 1) || ...
                (sign(buffer(1)) == -1 && sign(buffer(2)) == 0 && sign(buffer(3)) == 1);
    end
end