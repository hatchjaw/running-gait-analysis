function result = isZeroCrossing(buffer)
    if sign(buffer(end)) == -1
        result = sign(buffer(end-1)) == 1 || ...
            (sign(buffer(end-1)) == 0 && sign(buffer(end-2)) == 1);
    else
        result = sign(buffer(end-1)) == -1 || ...
            (sign(buffer(end-1)) == 0 && sign(buffer(end-2)) == -1);
    end
end

