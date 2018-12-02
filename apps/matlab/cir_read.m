tcp = tcpclient('127.0.0.1', 19021);
pause(1)
      
ntime = 120000
idx =[];
data=[];
idx =[];
utime_a =[];
utime_b =[];
idx_a =[];
idx_b =[];
cir =[];
cir_a =[];
cir_b =[];

for j=1:ntime
   
    data = [data, read(tcp)];
    str = char(data);
    idx = find(str == 13);
    
    while (length(idx) < 4)
        data = [data, read(tcp)];
        str = char(data);
        idx = find(str == 13);
    end    
            
    for i=1:length(idx) - 1
        line = str((idx(i)+2):idx(i+1));
        if (line(1) == '{')
            line = jsondecode(line);
            if (isstruct(line) && isfield(line,'utime')) 
                if (isfield(line,'cir'))
                    utime_a(end+1,:) = line.utime;
                    cir(end+1,:) = complex(line.cir.real,line.cir.imag)';
                    idx(end+1) = line.cir.idx;
                end
                if (isfield(line,'cir0'))
                    utime_a(end+1,:) = line.utime;
                    cir_a(end+1,:) = complex(line.cir0.real,line.cir0.imag)';
                    idx_a(end+1) = line.cir0.idx;
                end
                if (isfield(line,'cir1'))
                    utime_b(end+1,:) = line.utime;
                    cir_b(end+1,:) = complex(line.cir1.real,line.cir1.imag)';
                    idx_b(end+1) = line.cir1.idx;
                end     
            end
        end
    end
    data = data(idx(end):end);
    
    n=0;m=0;
    if(~isempty(cir))
        n=1:length(cir(end,:));
    end
    if(~isempty(cir_a))
        n=1:length(cir_a(end,:));
    end
    if(~isempty(cir_b))
        m=1:length(cir_b(end,:));
    end
    

    ax =[];
    ax(end+1) = subplot(311); 
    if (n == m)   
       plot(n,[real(cir_a(end,:));imag(cir_a(end,:))],'-o',m,[real(cir_b(end,:));imag(cir_b(end,:))]','-+');
    else
       plot(n,[real(cir(end,:));imag(cir(end,:))],'-o');
    end
    
       
    ax(end+1) = subplot(312); plot([angle(cir_a(end,:));angle(cir_b(end,:))]','-+');
    ax(end+1) = subplot(313); plot([abs(cir_a(end,:));abs(cir_b(end,:))]','-+');
    linkaxes(ax,'x');

end





