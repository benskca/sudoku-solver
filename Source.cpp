#include <iostream>
#include "SDL_ttf.h"
#include "SDL.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>

const Uint8* keys = SDL_GetKeyboardState(NULL);  // global constant tracking inputs
const int margin = 15;

class Column;

class Node
{
public:
	Node* L;
	Node* R;
	Node* U;
	Node* D;
	Column* C;
	Node(Node* l, Node* r, Node* u, Node* d, Column* c)
	{
		L = l;
		R = r;
		U = u;
		D = d;
		C = c;
	}
};

class Column : public Node
{
public:
	int N;
	int S;
	Column(Node* l, Node* r, Node* u, Node* d, int n, int s) : Node(l, r, u, d, this)
	{
		N = n;
		S = s;
	}
};

// draws the sudoku grid outline
void drawGrid(SDL_Renderer* ren)
{
	SDL_SetRenderDrawColor(ren, 255, 255, 255, 0);
	SDL_Rect clearRect{ margin, margin, 450, 450 };
	SDL_RenderFillRect(ren, &clearRect);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);

	for (int i{ 0 }; i < 10; i++)
	{
		SDL_RenderDrawLine(ren, margin + i * 50, margin, margin + i * 50, margin + 450);
		SDL_RenderDrawLine(ren, margin, margin + i * 50, margin + 450, margin + i * 50);
		if (i % 3 == 0)
		{
			SDL_RenderDrawLine(ren, margin + i * 50 - 1, margin, margin + i * 50 - 1, margin + 450);
			SDL_RenderDrawLine(ren, margin, margin + i * 50 - 1, margin + 450, margin + i * 50 - 1);
			SDL_RenderDrawLine(ren, margin + i * 50 + 1, margin, margin + i * 50 + 1, margin + 450);
			SDL_RenderDrawLine(ren, margin, margin + i * 50 + 1, margin + 450, margin + i * 50 + 1);
		}
	}
}

// this constructs the toroidal doubly linked list structure needed for dancing links from a given boolean matrix
Column* constructDancingLinksFromMatrix(bool** matrix, int dim1, int dim2)
{
	// construct an empty matrix
	Node*** nodeMatrix = new Node**[dim1];
	for (int i = 0; i < dim1; i++)
	{
		nodeMatrix[i] = new Node*[dim2];
		for (int j = 0; j < dim2; j++)
		{
			nodeMatrix[i][j] = nullptr;
		}
	}

	Column* headerColumn = new Column(nullptr, nullptr, nullptr, nullptr, -1, 0);
	
	// first put the columns in the top row
	for (int i = 0; i < dim2; i++)
	{
		nodeMatrix[0][i] = (Node*)new Column(nullptr, nullptr, nullptr, nullptr, i, 0);
	}

	// now populate the rest of the matrix with nodes
	for (int x = 0; x < dim2; x++)
	{
		for (int y = 1; y < dim1; y++)
		{
			if (y == 0 || matrix[y - 1][x] == 1)
			{
				Node* desiredColumnNode = nodeMatrix[0][x];
				nodeMatrix[y][x] = new Node(nullptr, nullptr, nullptr, nullptr, (Column*) desiredColumnNode);
			}
		}
	}

	// now connect up the nodes
	for (int y = 0; y < dim1; y++)
	{
		for (int x = 0; x < dim2; x++)
		{
			Node* firstSeenInRow = nullptr;
			if (nodeMatrix[y][x] != nullptr)
			{
				if (firstSeenInRow == nullptr) // store the first non-null pointer in the row
				{
					firstSeenInRow = nodeMatrix[y][x];
				}
				// loop down and connect to nearest node
				int i = (y + 1) % dim1;
				while (true)
				{
					if (nodeMatrix[i][x] != nullptr)
					{
						nodeMatrix[y][x]->D = nodeMatrix[i][x];
						nodeMatrix[i][x]->U = nodeMatrix[y][x];
						break;
					}
					i = (i + 1) % dim1;
				}
				// loop right and connect to nearest node
				int j = (x + 1) % dim2;
				while (true)
				{
					if (nodeMatrix[y][j] != nullptr)
					{
						nodeMatrix[y][x]->R = nodeMatrix[y][j];
						nodeMatrix[y][j]->L = nodeMatrix[y][x];
						break;
					}
					j = (j + 1) % dim2;
				}
			}
		}
	}

	// now connect in the header
	nodeMatrix[0][0]->L = headerColumn;
	headerColumn->R = nodeMatrix[0][0];
	nodeMatrix[0][dim2 - 1]->R = headerColumn;
	headerColumn->L = nodeMatrix[0][dim2 - 1];

	// now delete the matrix, as it was only needed to link the nodes together
	for (int i = 0; i < dim1; i++)
	{
		delete[] nodeMatrix[i];
	}
	delete[] nodeMatrix;

	return headerColumn;
}

// this removes a column from the dancing links structure
void coverColumn(Node* c)
{
	// connect the columns on either side of this one
	c->R->L = c->L;
	c->L->R = c->R;
	// remove every row in which this column has a 1
	Node* i = c->D;
	while (i != c)
	{
		Node* j = i->R;
		while (j != i)
		{
			j->D->U = j->U;
			j->U->D = j->D;
			j = j->R;
		}
		i = i->D;
	}
}

// this readds a column into the dancing links structure
void uncoverColumn(Node* c)
{
	Node* i = c->U;
	// readd the rows
	while (i != c)
	{
		Node* j = i->L;
		while (j != i)
		{
			j->D->U = j;
			j->U->D = j;
			j = j->L;
		}
		i = i->U;
	}
	// reinsert the column between its neighbours
	c->L->R = c;
	c->R->L = c;
}

// the dancing links search function
int search(Node** solution, Node* h, int k)
{
	Node* c = h->R;
	if (c == h)
	{
		return 1;
	}
	coverColumn(c);
	Node* r = c->D;
	while (r != c)
	{
		solution[k] = r;
		Node* j = r->R;
		while (j != r)
		{
			coverColumn(j->C);
			j = j->R;
		}
		if (search(solution, h, k + 1) == 1)
		{
			return 1;
		}
		j = r->L;
		while (j != r)
		{
			uncoverColumn(j->C);
			j = j->L;
		}
		r = r->D;
	}
	uncoverColumn(c);
	return 0;
}

void solveSudokuDancingLinks(Column* header, Node** solution, int* startingGrid)
{
	int solutionCount = 0;

	// first add the numbers already present in the grid to the solution
	Node* correspondingNodesToRemove[81]{ nullptr };
	int back = 0;
	for (int y = 0; y < 9; y++)
	{
		for (int x = 0; x < 9; x++)
		{
			int n = startingGrid[9 * y + x] - 1;
			if (n != -1)
			{
				// go to the node corresponding to this number existing at this position
				Node* correspondingNode = header;
				for (int i = 0; i < 81 + 9 * n + x + 1; i++) // go to collumn corresponding to the digit n in the x'th column (+1 for header)
				{
					correspondingNode = correspondingNode->R;
				}
				for (int j = 0; j < y + 1; j++) // go to the (y+1)'th node down (+1 to get out of column list)
				{
					correspondingNode = correspondingNode->D;
				}
				correspondingNodesToRemove[back] = correspondingNode;
				back++;
			}
		}
	}
	for (int i = 0; i < back; i++)
	{
		Node* correspondingNode = correspondingNodesToRemove[i];
		// cover the corresponding columns				
		coverColumn(correspondingNode->C);
		Node* j = correspondingNode->R;
		while (j != correspondingNode)
		{
			coverColumn(j->C);
			j = j->R;
		}
		solution[solutionCount] = correspondingNode; // add to solution
		solutionCount++;
	}

	// now perform the usual dancing links algorithm on the reduced system
	search(solution, header, solutionCount);
}

SDL_Texture* numTexts[9];

int main(int, char**)
{
	// SETUP

	// creates full constraint matrix for a sudoku
	// NB: A[choice][constraint]
	bool** A = new bool*[729];
	for (int i = 0; i < 729; i++)
	{
		A[i] = new bool[324];
		for (int j = 0; j < 324; j++)
		{
			A[i][j] = 0;
		}
	}

	// NB: (n, x, y) = digit (n+1) at position (x, y). This is choice 81*n + 9*x + y
	for (int n = 0; n < 9; n++)
	{
		for (int x = 0; x < 9; x++)
		{
			for (int y = 0; y < 9; y++)
			{
				int choice = 81 * n + 9 * x + y;
				int b = x / 3 + (y / 3) * 3; // which box the choice lies in
				A[choice][9 * x + y] = 1;			// satisfies "a number must exist at (x, y)
				A[choice][81 + 9 * n + x] = 1;		// satisfies "the number n must exist in column x"
				A[choice][162 + 9 * n + y] = 1;		// satisfies "the number n must exist in row y"
				A[choice][243 + 9 * n + b] = 1;		// satisfies "the number n must exist in box b"
			}
		}
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		return 1;
	}

	// load font
	TTF_Init();
	TTF_Font* font = TTF_OpenFont("calibri regular.ttf", 35);
	TTF_Font* fontSmall = TTF_OpenFont("calibri regular.ttf", 20);
	int textRectH;
	int textRectW;
	TTF_SizeText(font, "2", &textRectW, &textRectH);

	// create window and draw empty grid
	SDL_Window* win = SDL_CreateWindow("Sudoku Solver", 10, 10, 900 + 2 * margin, 450 + 2 * margin, SDL_WINDOW_SHOWN);
	SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawColor(ren, 245, 245, 245, 255);
	SDL_RenderClear(ren);
	drawGrid(ren);

	// create number textures
	for (int i{ 1 }; i < std::size(numTexts) + 1; i++)
	{
		std::string valString{ std::to_string(i) };
		numTexts[i-1] = SDL_CreateTextureFromSurface(ren, TTF_RenderText_Shaded(font, valString.c_str(), { 0, 0, 0 }, { 255, 255, 255 }));
	}

	const char instructionString[]{ "This sudoku solver reposes the sudoku as an exact cover problem and then solves this using Donald Knuth's dancing links algorithm.\n\nINSTRUCTIONS\n1-9: enter number into grid\nc: remove number\nSpace: solve the sudoku\nBackspace: reset the board" };
	SDL_Surface* instructionSurface{ TTF_RenderText_Blended_Wrapped(fontSmall, instructionString, { 0, 0, 0 }, 400) };
	SDL_Texture* instructionsText = SDL_CreateTextureFromSurface(ren, instructionSurface);
	SDL_Rect instructionsRect{ margin + 475, margin + 10, instructionSurface->w, instructionSurface->h };
	SDL_RenderCopy(ren, instructionsText, NULL, &instructionsRect);

	// create the empty display grid
	int sudokuGrid[81]{ 0 };
	int mousex{ 0 };
	int mousey{ 0 };

	SDL_Event event;

	// LOOP

	int solve{ 0 };
	bool running{ true };
	while (running)
	{
		SDL_GetMouseState(&mousex, &mousey);

		// get mouse position in terms of sudoku grid
		mousex = floor((mousex - margin) / 50);
		mousey = floor((mousey - margin) / 50);

		if (mousex < 0 || mousex > 8 || mousey < 0 || mousey > 8)
		{
			mousex = -1; // ignore mouse pos if not over the grid
		}

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_KEYDOWN)
			{
				const char* key = SDL_GetKeyName(SDL_GetKeyFromScancode(event.key.keysym.scancode));
				int enteredNum{ static_cast<int>(*key) - 48 };
				if (mousex != -1) // if mouse on grid
				{
					if (enteredNum > 0 && enteredNum < 10) // if a digit 1-9 pressed
					{
						// update grid with new number
						sudokuGrid[mousey * 9 + mousex] = enteredNum;
						// draw number
						SDL_Rect textRect{ margin + mousex * 50 + 16, margin + mousey * 50 + 10, textRectW, textRectH };
						SDL_RenderCopy(ren, numTexts[enteredNum - 1], NULL, &textRect);
					}
					else if (enteredNum == 19) // if c pressed
					{
						// remove number from grid
						sudokuGrid[mousey * 9 + mousex] = 0;
						SDL_Rect textRect{ margin + mousex * 50 + 16, margin + mousey * 50 + 10, textRectW, textRectH };
						SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
						SDL_RenderFillRect(ren, &textRect);
					}
				}
				if (enteredNum == 35)  // if space pressed
				{
					if (solve == 0) solve = 1; // start solving
				}
				else if (enteredNum == 18)  // if backspace pressed
				{
					// clear grid
					for (int &n : sudokuGrid)
					{
						n = 0;
					}
					drawGrid(ren);
				}
			}
			else if (event.type == SDL_QUIT) // if exit clicked
			{
				running = false; // stop the program
			}
		}

		if (solve) // if sudoku in grid is to be solved
		{
			Column* header = constructDancingLinksFromMatrix(A, 730, 324);
			Node* solution[81]{ nullptr };
			solveSudokuDancingLinks(header, solution, sudokuGrid);
			// solution is returned as a list of nodes, now need to reconstruct display grid format
			for (int i = 0; i < 81; i++)
			{
				// read off the column index of all the nodes in the same row as this one
				std::vector<int> solutionNumbers;
				Node* node = solution[i];
				Node* j = node;
				do
				{
					solutionNumbers.push_back(j->C->N);
					j = j->R;
				} while (j != node);
				// sort in asending order
				std::sort(solutionNumbers.begin(), solutionNumbers.end());

				// recover digit and position (this undoes the index manipulation used to populate the array at the start of the program)
				int x = (solutionNumbers[0] + solutionNumbers[1] - solutionNumbers[2] + 81) / 10;
				int y = solutionNumbers[0] - 9 * x;
				int n = (solutionNumbers[1] - x - 81) / 9 + 1;

				// add to the display grid and draw to screen
				sudokuGrid[y * 9 + x] = n;
				SDL_Rect textRect{ margin + x * 50 + 16, margin + y * 50 + 10, textRectW, textRectH };
				SDL_RenderCopy(ren, numTexts[n - 1], NULL, &textRect);
			}
			solve = false;
		}

		SDL_RenderPresent(ren);
	}

	for (int i = 0; i < 729; i++)
	{
		delete[] A[i];
	}
	delete[] A;


	for (SDL_Texture* text : numTexts)
	{
		SDL_DestroyTexture(text);
	}
	SDL_DestroyTexture(instructionsText);

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	TTF_Quit();
	SDL_Quit();

	return 0;
}
