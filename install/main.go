package main

import (
	"bufio"
	"bytes"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"

	"github.com/go-git/go-git/v5"
	"github.com/rivo/tview"
)

const (
	localInstallDir     = "./ouroveon"
	versionIdentityFile = "./ouroveon/VERSION"
	githubBuildRepoURL  = "https://github.com/Unbundlesss/OUROVEON-build"
	githubVersionFile   = "https://raw.githubusercontent.com/Unbundlesss/OUROVEON-build/main/VERSION"
)

func tryReadVersionFile() (string, error) {

	// doesn't exist? just return an empty string and no error
	_, err := os.Stat(versionIdentityFile)
	if err != nil {
		if os.IsNotExist(err) {
			return "", nil
		}
		return "", err
	}

	// read the version file
	versionFile, err := os.Open(versionIdentityFile)
	if err != nil {
		return "", err
	}

	defer versionFile.Close()

	scanner := bufio.NewScanner(versionFile)
	scanner.Scan()

	if err := scanner.Err(); err != nil {
		return "", err
	}

	return scanner.Text(), nil
}

func TryFetchNewVersionFromGitHub() (string, error) {

	resp, err := http.Get(githubVersionFile)
	if err != nil {
		return "", err
	}
	// read the version text from the http response
	scanner := bufio.NewScanner(resp.Body)
	scanner.Scan()
	if err := scanner.Err(); err != nil {
		return "", err
	}

	return scanner.Text(), nil
}

func WriteStringLog(txt string) {
	file, err := os.Create("install.log.txt")
	if err != nil {
		return
	}
	file.WriteString(txt)
	file.Close()
}

func FreshClone() (string, error) {
	var writer bytes.Buffer
	_, err := git.PlainClone(localInstallDir, false, &git.CloneOptions{
		URL:      githubBuildRepoURL,
		Progress: &writer,
	})
	return writer.String(), err
}

func AttemptSync() (string, error) {
	var writer bytes.Buffer
	// reset the local repo and pull latest
	repo, err := git.PlainOpen(localInstallDir)
	if err != nil {
		return "", err
	}
	w, err := repo.Worktree()
	if err != nil {
		return "", err
	}
	err = w.Reset(&git.ResetOptions{
		Mode: git.HardReset,
	})
	if err != nil {
		return "", err
	}
	err = w.Pull(&git.PullOptions{
		Force:    true,
		Progress: &writer,
	})
	if err != nil {
		return "", err
	}
	return writer.String(), nil
}

func HandleFailableCall(pages *tview.Pages, app *tview.Application, log string, err error) {

	// write results to file log just in case
	WriteStringLog(log)

	// create an error page?
	if err != nil {
		pages.AddPage("Error",
			tview.NewModal().
				SetText("There was a problem:\n\n"+err.Error()).
				AddButtons([]string{"Ok Thanks"}).
				SetDoneFunc(func(buttonIndex int, buttonLabel string) {
					app.Stop()
				}),
			false,
			false)
		pages.SwitchToPage("Error")
	} else {
		pages.SwitchToPage("Complete")
	}
}

func main() {

	// see if the local install directory exists at all
	localDirExists := true
	if _, err := os.Stat(localInstallDir); os.IsNotExist(err) {
		localDirExists = false
	}
	fullAbsoluteInstallPath, err := filepath.Abs(localInstallDir)
	if err != nil {
		log.Fatal(err)
	}

	// if the local install directory exists, check if the version file exists
	currentKnownVersion, err := tryReadVersionFile()
	if err != nil {
		log.Println("Cannot read version file :", err)
	}

	onlineKnownVersion, err := TryFetchNewVersionFromGitHub()
	if err != nil {
		log.Println("Unable to fetch current version from GitHub :", err)
	}

	app := tview.NewApplication()
	pages := tview.NewPages()

	pages.AddPage("Intro",
		tview.NewModal().
			SetText("OUROVEON Installer & Updater\n\nThis tool will create or sync a local install of the OUROVEON tools").
			AddButtons([]string{"Begin", "Quit"}).
			SetDoneFunc(func(buttonIndex int, buttonLabel string) {
				if buttonIndex == 0 {
					pages.SwitchToPage("Phase1")
				} else {
					app.Stop()
				}
			}),
		false,
		true)

	pages.AddPage("Complete",
		tview.NewModal().
			SetText("All done. Have fun!\nishani xx").
			AddButtons([]string{"Quit"}).
			SetDoneFunc(func(buttonIndex int, buttonLabel string) {
				app.Stop()
			}),
		false,
		false)

	// if nothing exists yet, offer up a clone option
	if !localDirExists {

		pages.AddPage("Phase1",
			tview.NewModal().
				SetText(fmt.Sprintf("Version %s is available online.\nNo local install is found. It will be placed next to this updater, in :\n%s\n", onlineKnownVersion, fullAbsoluteInstallPath)).
				AddButtons([]string{"Create", "Quit"}).
				SetDoneFunc(func(buttonIndex int, buttonLabel string) {
					if buttonIndex == 0 {
						consoleLog, err := FreshClone()
						HandleFailableCall(pages, app, consoleLog, err)
					} else {
						app.Stop()
					}
				}),
			false,
			false)

		// otherwise we're likely to be updating
	} else {

		if currentKnownVersion == "" {

			pages.AddPage("Phase1",
				tview.NewModal().
					SetText("Cannot find the VERSION file in the local install.\nSync may fail - delete the install and retry if so.").
					AddButtons([]string{"Sync", "Quit"}).
					SetDoneFunc(func(buttonIndex int, buttonLabel string) {
						if buttonIndex == 0 {
							consoleLog, err := AttemptSync()
							HandleFailableCall(pages, app, consoleLog, err)
						} else {
							app.Stop()
						}
					}),
				false,
				false)

		} else {

			pages.AddPage("Phase1",
				tview.NewModal().
					SetText(fmt.Sprintf("Current version : %s\nOnline version : %s\n\nChoose SYNC to continue, it may take a moment to update.\n", currentKnownVersion, onlineKnownVersion)).
					AddButtons([]string{"Sync", "Quit"}).
					SetDoneFunc(func(buttonIndex int, buttonLabel string) {
						if buttonIndex == 0 {
							consoleLog, err := AttemptSync()
							HandleFailableCall(pages, app, consoleLog, err)
						} else {
							app.Stop()
						}
					}),
				false,
				false)

		}

	}

	if err := app.SetRoot(pages, true).SetFocus(pages).Run(); err != nil {
		panic(err)
	}
}
