function indicateEvents(events, eventLabel, eventMarker, intervalLabel, xOffset, yPos, tVec, n, lookaround)
%INDICATEEVENTS 
    if ~isempty(events)
        scatter(events(:, 1), events(:, 2), 100, eventMarker);
        eventsToLabel = events(events(:, 1) > tVec(n-lookaround, :), :);
        for t=1:size(eventsToLabel, 1)
            label = eventLabel;
            if eventsToLabel(t, 3) == 1
                label = label + " L";
            else
                label = label + " R";
            end
            text(eventsToLabel(t, 1) - .05, eventsToLabel(t, 2) + .2, label);
        end
        
        % Display inter-toe-off interval and balance
        if size(events, 1) > 1
            L = mean(events(events(:, 3) == 1 & events(:, 4) < 1, 4));
            R = mean(events(events(:, 3) == -1 & events(:, 4) < 1, 4));
            BL = L/(L+R);
            BR = R/(L+R);
            text(tVec(n) + xOffset, ...
                yPos, ...
                { ...
                    "$" + intervalLabel + " = " + num2str(events(end, 4)) + "$", ...
                    "$B_L = " + num2str(BL * 100) + "\%$", ...
                    "$B_R = " + num2str(BR * 100) + "\%$"
                }, ...
                'interpreter', 'latex', 'FontSize', 16 ...
            );
        end
end
end

