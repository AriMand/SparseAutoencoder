function [cost,grad] = sparseAutoencoderCost(W, visibleSize, hiddenSize, ...
    lambda, sparsityParam, beta, data)

% visibleSize: ���������� ������� ����� (probably 64)
% hiddenSize: ���������� ������� ����� (probably 25)
% lambda: ����������� ���������� �����
% sparsityParam: �������� ������� ��������� �������� �������� ���� (��).
% beta: ����������� (���) ���������� ����������� �� �������������.
% data: ���� ������� 64x10000 ���������� ��������� �������.
% ����� �������, data(:,i) ��� i-th ��������� ���� (���� � �����, � ������ ������ ���� � ��-��).

% ������� �������� W ��� ������ (�.�. minFunc �������, ��� �������� �������� ��������).
% ������� �� �������� W �� ����� (W1, W2, b1, b2), ����� ��� ���� ��� � ������.

% ����, ����������� ������� ������ � ������� ����
W1 = reshape(W(1:hiddenSize*visibleSize), hiddenSize, visibleSize);
% ����, ����������� ������� ���� � �����
W2 = reshape(W(hiddenSize*visibleSize+1:2*hiddenSize*visibleSize), visibleSize, hiddenSize);
% �������� �������� �������� ����
b1 = W(2*hiddenSize*visibleSize+1:2*hiddenSize*visibleSize+hiddenSize);
% �������� �������� ��������� ����
b2 = W(2*hiddenSize*visibleSize+hiddenSize+1:end);

% ������� ��������� � ��������� (��� ��� ������ ���������� ��� ��������).
% ��� ��� ���������������� ������.
W1grad = zeros(size(W1));
W2grad = zeros(size(W2));
b1grad = zeros(size(b1));
b2grad = zeros(size(b2));

% ---------- ��� ��� ����� --------------------------------------
%  ����������: ��������� ������� ������/������� ����������� J_sparse(W,b) ��� ������������ �����������,
%              � ��������������� ��������� W1grad, W2grad, b1grad, b2grad.
%
% W1grad, W2grad, b1grad � b2grad ����������� ������� ��������� ��������������� ������.
% �������, ��� W1grad ������ ����� �� �� ������� ��� � W1,
% b1grad ������ ����� �� �� ������� ��� � b1, � �.�.
% W1grad ��� ������� ����������� J_sparse(W,b) �� W1.
% �.�., W1grad(i,j) ��� ������� ����������� J_sparse(W,b)
% �� ��������� W1(i,j).  ����� �������, W1grad ������ ���� �����
% [(1/m) Delta W1 + lambda W1] � ��������� ����� ���������� ������ 2.2
% ������ (� ���������� ��� W2grad, b1grad, b2grad).
%
% ������� �������, ���� �� ���������� �������� ����� ������������ ������,
% �� ������ ���� W1 ����� ���������� �� �������: W1 := W1 - alpha * W1grad,
% ���������� ��� W2, b1, b2.
%
% i - ����� ��������� (������� ������)

numPatches=size(data,2);

avgActivations=zeros(size(W1,1),1);
storedHiddenValues = zeros(hiddenSize, numPatches);
storedOutputValues = zeros(visibleSize, numPatches);
J=0;
%----------------------------
% ������ ������ (������ ������ ����)
%----------------------------
for i=1:numPatches
    X=data(:,i);
    z2=W1*X+b1;
    a2=sigmoid(z2);
    avgActivations=avgActivations+a2;
    z3=W2*a2+b2;
    a3=sigmoid(z3);
    % �������� ���������� ������� ����
    storedHiddenValues(:, i) = a2;
    storedOutputValues(:, i) = a3;
    % ��������� ������� ������ (����� ��������� ������)
    J=J+0.5*sum(sum((a3-X).^2));
end
%----------------------------
% ����������, ��������� � �������� �������������
% �� ���� ����� � ������� ���� �� �������� ������
% �������������� ���� ��������� ���������� ��������
%----------------------------

% �� ��������� ����� ������ �������
avgActivations=avgActivations./numPatches;
% ����������� � ������ �������� ���� ��� �������� �������
sparsity_grad=beta.*(-sparsityParam./avgActivations+((1-sparsityParam)./(1-avgActivations)));

% ��������� ����������� ��������-��������
KL1=sparsityParam*log(sparsityParam./avgActivations);
KL2=(1-sparsityParam)*log((1-sparsityParam)./(1-avgActivations));
% ����������� ��������-�������� (����� ��������� �� ���� ������� ������)
KL_divergence=sum(sum(KL1+KL2));
% ������� ������ (�������������� ����������)
cost=(1/numPatches)*J+lambda*0.5*(sum(sum(W1.^2))+sum(sum(W2.^2)))+beta*KL_divergence;
%----------------------------
% �������� ��������������� ������
%----------------------------
for i=1:numPatches
    X=data(:,i);
    % ������� ����� ����������� ����
    a2 = storedHiddenValues(:, i);
    a3 = storedOutputValues(:, i);
    % ������ ��������� ����
    delta_3=(a3-X).*a3.*(1-a3);
    % ������ �������� ����
    delta_2=(W2'*delta_3+sparsity_grad).*a2.*(1-a2);
    
    W1grad=W1grad+delta_2*X';
    W2grad=W2grad+delta_3*a2';
    
    b1grad=b1grad+delta_2;
    b2grad=b2grad+delta_3;
end

%----------------------------
% ��������� �����
%----------------------------
W1grad=(1/numPatches).*W1grad+(lambda).*W1;
W2grad=(1/numPatches).*W2grad+(lambda).*W2;
%----------------------------
% ��������� ��������
%----------------------------
b1grad = (1/numPatches).*b1grad;
b2grad = (1/numPatches).*b2grad;
%----------------------------
% ������� ����������� ��������
% ���������� � ������-�������
% (���������� ��� minFunc).
%----------------------------
grad = [W1grad(:) ; W2grad(:) ; b1grad(:) ; b2grad(:)];

end

%-------------------------------------------------------------------
% ������� ���������� ��������
%-------------------------------------------------------------------
function sigm = sigmoid(x)
sigm = 1 ./ (1 + exp(-x));
end