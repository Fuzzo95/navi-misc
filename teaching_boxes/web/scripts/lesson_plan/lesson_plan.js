//------------------------------------
// Class LessonPlan
//------------------------------------

// Constructor for the LessonPlan object
function LessonPlan (name, desc)
{
	// Members
	this.fields  = new Array ()
	this.mainDiv = document.getElementById ('mainDiv')
	this.toolbar = top.document.getElementById ('toolbar')
    this.name    = name
    this.desc    = desc
    this.id      = -1

	// Methods
	this.addField     = addField
    this.getField     = getField
	this.setupToolbar = setupToolbar
	this.addLink	  = addLink
	this.saveYourself = saveYourself
    this.serialize    = lessonSerialize
    this.createNew    = lessonCreateNew

	// Toolbar
	this.setupToolbar (this)

    // Fields
    for (item in requiredFields)
    {
        this.addField (item, requiredFields[item], false, false)
    }
}

function lessonCreateNew ()
{
    if (confirm ("This action will save the current lesson and create a new one.  Continue?"))
    {
        var http = new XMLHttpRequest ()

        saveID = function ()
        {
            if (http.readyState == 4)
            {
                lessonPlan.id = http.responseText
                alert ("Lesson saved successfully.")
                top.window.location = "new_lesson.html"
            }
        }

        http.open ('post', 'cgi-bin/dosomething.py?saveLesson')
        http.onreadystatechange = saveID
        http.send (lessonPlan.serialize ())
    }
}

function saveYourself ()
{
	var http = new XMLHttpRequest ()

    if (lessonPlan.getField("Title").value.length == 0)
    {
        alert ("You must fill in at least the \"Title\" field.")
        return
    }

    saveID = function ()
    {
        if (http.readyState == 4)
        {
            lessonPlan.id = http.responseText
            alert ("Lesson saved successfully.")
        }
    }

	http.open ('post', 'cgi-bin/dosomething.py?saveLesson')
	http.onreadystatechange = saveID
	http.send (lessonPlan.serialize ())
}

function lessonSerialize ()
{
    name = lessonPlan.getField ("Title").value
    desc = lessonPlan.getField ("Description").value

    return "" + lessonPlan.id + "|" + name + "|" + desc
}

function addLink (fieldName, id, name, desc, linkType, addr)
{
	var field = document.getElementById (fieldName)
	field.linkObj.addLink (id, name, desc, linkType, addr)
}

function borderSwitch (e)
{
	tg = e.target
	while (!tg.highlightable)
	{
		if (!tg.parentNode)
			return

		tg = tg.parentNode
	}

	tmp = tg.style.backgroundColor
	tg.style.backgroundColor = tg.activeColor
	tg.activeColor = tmp
}

function showToolTip (x, y, counter, tip)
{
	if (tipVisible || counter != currentFocus)
		return

	tipVisible = true
	div = document.createElement ("div")
	div.setAttribute ("id", "tooltip")
	div.style.backgroundColor = "#eeeeee"
	div.style.border = "thin solid black"
	div.style.padding = "5px"
	div.appendChild (document.createTextNode (tip))
	div.style.position = "absolute"
	div.style.top = "" + (y + 10) + "px"
	div.style.left = "" + (x + 10) + "px"
	top.document.body.appendChild (div)
}

function backgroundIn (e)
{
	tg = e.target.mouseObj
	/*while (!tg.highlightable)
	{
		if (!tg.parentNode)
			return

		tg = tg.parentNode
	}*/

	if (!tg)
		return

	tg.style.borderLeft = tg.style.borderTop = "1px solid #5555aa"
	tg.style.borderRight = tg.style.borderBottom = "1px solid #5555aa"
	tg.style.backgroundColor = tg.sensitiveColor

	if (!tg.tipVisible)
	{
		focusCounter ++
		currentFocus = focusCounter
		tg.tipVisible = true
		x = e.clientX
		y = e.clientY
		setTimeout ("showToolTip (" + x + ", " + y + ", " + focusCounter + ", \"" + tg.tip + "\");", 500)
	}
}

function backgroundOut (e)
{
	tg = e.target.mouseObj
	/*
	while (!tg.highlightable)
	{
		if (!tg.parentNode)
			return
		tg = tg.parentNode
	}
	*/

	if (!tg)
		return

	tg.style.borderLeft = tg.style.borderTop = "1px solid #cccccc"
	tg.style.borderRight = tg.style.borderBottom = "1px solid #cccccc"
	tg.style.backgroundColor = "#cccccc"
	tg.focused = false

	if (tg.tipVisible)
	{
		currentFocus = -1
		tg.tipVisible = false
		tipVisible = false
		div = top.document.getElementById ("tooltip")
		if (div)
			top.document.body.removeChild (div)
	}
}

function createButton (mytr, name, tip, icon, onClick)
{
	mytd = document.createElement ("td")
	mytr.appendChild (mytd)

	table = document.createElement ("table")
	table.mouseObj = mytd
	mytd.appendChild (table)

	tr = document.createElement ("tr")
	tr.mouseObj = mytd
	table.appendChild (tr)

	td = document.createElement ("td")
	td.mouseObj = mytd
	tr.appendChild (td)
	img = document.createElement ("img")
	img.mouseObj = mytd
	td.appendChild (img)
	img.setAttribute ("src", icon)

	td1 = document.createElement ("td")
	td1.mouseObj = mytd
	tr.appendChild (td1)
	td1.appendChild (document.createTextNode (name))

	mytd.style.cursor = "default"
	mytd.setAttribute ('valign', "middle")
	mytd.style.borderLeft = mytd.style.borderTop = "1px solid #cccccc"
	mytd.style.borderRight = mytd.style.borderBottom = "1px solid #cccccc"
	mytd.sensitiveColor = "#aaaaee"
	mytd.activeColor = "#8888bb"
	mytd.onmouseover = backgroundIn
	mytd.onmouseout = backgroundOut
	mytd.onmousedown = borderSwitch
	mytd.onmouseup = borderSwitch
	mytd.onclick = onClick
	mytd.mouseObj = mytd
	mytd.highlightable = true
	mytd.tip = tip
	return mytr
}

function setupToolbar (plan)
{
	tab = document.createElement ("table")
	tr = document.createElement ("tr")

	tr = createButton (tr,
		"New Lesson",
		"Create a new lesson and add it to the box.",
		STOCK_NEW,
        function (e) {lessonPlan.createNew ()});

	tr = createButton (tr,
		"Save Lesson",
		"Save the changes to the current lesson.",
		STOCK_SAVE,
        function (e) {lessonPlan.saveYourself ()});

	tr = createButton (tr,
		"Create Link",
		"Add a link to another lesson",
		STOCK_LINK,
		function (e) {addLink = new AddLinkDlg ()})

	tr = createButton (tr,
		"Add Field",
		"Add additional information to this lesson.",
		STOCK_FIELD,
		function (e) {addField = new AddFieldDlg ()})

	tr = createButton (tr,
		"Add Note",
		"Add a personal note or reminder to the lesson.",
		STOCK_NOTE)

	tr = createButton (tr,
		"Add to Box",
		"Add this lesson to a teaching box",
		STOCK_BOX,
		function (e) {addToBox = new AddToBoxDlg ()})

	tab.appendChild (tr)
	plan.toolbar.appendChild (tab)
}

function uploadFile ()
{
}

// Add a field to the lesson plan
function addField (name, field, removable, mceReplace)
{
	newField = createField (name, field, removable)
	this.fields[newField.id] = newField
	this.mainDiv.appendChild (newField)

    if (mceReplace && this.getField(field['id']))
    {
        tinyMCE.execCommand ("mceAddControl", true, field['id'])
    }

	return newField
}

// Create a field and return it
function createField (name, field, removable)
{
	desc = field['desc']
	type = field['type']

	// Create the title of the field
	title = document.createElement ("span")
	text  = document.createTextNode (name)

	title.setAttribute ("class", "fieldName")
	title.appendChild (text)

	date = new Date ()
	id = new String (date.getTime ())
	var rmL = null

	if (removable) {
		rm = document.createElement ("span")
		rm.setAttribute ("class", "removeButton")
		rm.setAttribute ("id", id)
		rm.appendChild (document.createTextNode ("Remove"))
		rm.onclick = removeField
	}

	// Create the description tag
	descTag = document.createElement ("div")
	descTag.setAttribute ("class", "descriptionField")
	descTag.appendChild (document.createTextNode (desc))

	// Determine which type of field we want
	switch (type)
	{
		case TYPE_ENTRY:
			entry = document.createElement ("input")
            entry.id = field['id']
			entry.setAttribute ("type", "text")
			entry.style.width = "90%"
			entry.style.margin = "20px"
			entry.style.border = "thin solid #7777cc"
			entry.tabIndex = "-1"
			break
		case TYPE_TEXTAREA:
			entry = document.createElement ("div")
			entry.style.width = "90%"
			entry.style.margin = "20px"

			textarea = document.createElement ("textarea")
			entry.appendChild (textarea)
			textarea.setAttribute ("rows", 10)
			textarea.id = field['id']
			textarea.style.width = "100%"
			break
		case TYPE_LINK:
			rmL = document.createElement ("span")
			rmL.setAttribute ("class", "removeButton")
			rmL.setAttribute ("id", id)
			rmL.appendChild (document.createTextNode ("Remove All"))

			linkID = field['id']
			entry = document.createElement ("div")
			entry.style.width = "90%"
			entry.style.margin = "20px"
			entry.id = linkID
			entry.linkObj = new LinkField (entry, rmL)
			break
        case TYPE_FILELINK:
			rmL = document.createElement ("span")
			rmL.setAttribute ("class", "removeButton")
			rmL.setAttribute ("id", id)
			rmL.appendChild (document.createTextNode ("Remove All"))

			linkID = field['id']
			entry = document.createElement ("div")
			entry.style.width = "90%"
			entry.style.margin = "20px"
			entry.id = linkID
			entry.linkObj = new LinkField (entry, rmL, true)
			break
            
		default:
			entry = document.createElement ("textarea")
			break
	}

	// Package it all up and return it
	div = document.createElement ("div")
	div.setAttribute ("id", id)
	div.style.marginTop = "20px"
	div.appendChild (title)
	if (removable)
		div.appendChild (rm)

	if (rmL)
	{
		div.appendChild (rmL)
		div.linkObj = entry.linkObj
	}

	div.appendChild (document.createElement ("br"))
	div.appendChild (descTag)
	div.appendChild (entry)
	div.fieldName = name

	return div
}

function getField (field)
{
    return document.getElementById (field)
}

function removeField (event)
{
	if (confirm ("Are you sure you want to remove this field?")) {
		var id
		var mainDiv = document.getElementById ("mainDiv")

		if (event.target) {
			id = event.target.id
		} else {
			id = event.srcElement.id
		}

		fieldName = lessonPlan.fields[id].fieldName
		if (optionalFields[fieldName])
			optionalFields[fieldName]['used'] = false

		mainDiv.removeChild (lessonPlan.fields[id])
		delete lessonPlan.fields[id]
	}
}

//-----------------------------
// Main functions
//-----------------------------

// Handle window resizes
function calculateSizes ()
{
	// Setup the sizes of the navigation window and lesson plan
	// Get the size of the window
	bannerHeight = top.document.getElementById ('banner').height
	toolbarHeight = top.document.getElementById ('toolbar').clientHeight + 14
	navbarHeight = top.document.getElementById ('navigation').clientHeight - 5

	windowHeight = top.innerHeight - bannerHeight - toolbarHeight - navbarHeight - 0
	windowWidth = top.innerWidth

	// Set the height and width of stuff
	main = top.document.getElementById ('iframe')
	main.setAttribute ("style", "height: " + windowHeight)
}

function onCancelClick (event)
{
	newBox.style.display = "none"
	top.document.getElementById ("iframeDiv").style.display = "block"
    history.back ()
}

function onCreateClick (event)
{
	// Get the name and description fields
	var name = top.document.getElementById ("lessonName").value
	var desc = top.document.getElementById ("lessonDesc").value

	// Create the stuff.
	lessonPlan = new LessonPlan (name, desc)

	// Calculate the size of the window
	calculateSizes ()

    // Make everything visible again
	newBox.style.display = "none"
	top.document.getElementById ("iframeDiv").style.display = "block"

    // Show the Add Initial Fields dialog
    addInitFieldsDlg = new AddInitFieldsDlg ()
}

// Main function that does all the kickoff stuff
function main ()
{
	// Make sure the main window knows to call that every time we
	// resize.
	top.onresize = calculateSizes

    // Init MCE
    lessonPlan = new LessonPlan ("Untitled", "No description")

	// Calculate the size of the window
	calculateSizes ()

    addInitFieldsDlg = new AddInitFieldsDlg ()

	//var newCancel = top.document.getElementById ("newCancel")
	//newCancel.onclick = onCancelClick

	//var newCreate = top.document.getElementById ("newCreate")
	//newCreate.onclick = onCreateClick

}

tinyMCE.init ({
	mode: "textareas",
	theme: "simple"})

main ()

